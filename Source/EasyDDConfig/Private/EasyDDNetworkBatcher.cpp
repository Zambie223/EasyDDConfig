#include "EasyDDNetworkBatcher.h"

#include "Containers/Ticker.h"
#include "EasyDDConfigLog.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "FGCentralStorageSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "Patching/NativeHookManager.h"
#include "Resources/FGItemDescriptor.h"

namespace
{
	using FAmountRpc = void (AFGCentralStorageSubsystem::*)(TSubclassOf<UFGItemDescriptor>, int32);

	// The item-amount RPC is protected. A using-declaration in a derived type makes its address available; the
	// pointer still has the base class's member type. The type is never instantiated, and no access transformer
	// (which would rebuild FactoryGame) is needed.
	struct FCentralStorageRpcAccess : AFGCentralStorageSubsystem
	{
		using AFGCentralStorageSubsystem::Client_ItemAmountUpdatedInCentralStorage;
		static constexpr FAmountRpc AmountRpc = &FCentralStorageRpcAccess::Client_ItemAmountUpdatedInCentralStorage;
	};

	constexpr FAmountRpc AmountRpc = FCentralStorageRpcAccess::AmountRpc;
	using FAmountRpcHook = HookInvoker<FAmountRpc, AmountRpc>;
	constexpr const TCHAR* AmountRpcName = TEXT("AFGCentralStorageSubsystem::Client_ItemAmountUpdatedInCentralStorage");

	/** If this mod is installed it does the same job; hooking the RPC twice would bounce every message between the two. */
	constexpr const TCHAR* DepotNetworkFixPlugin = TEXT("DepotNetworkFix");

	/** Seconds between two sends of merged updates. */
	constexpr double FlushIntervalSeconds = 1.0;
	/** Seconds between two ticker runs. */
	constexpr float TickerIntervalSeconds = 0.1f;
	/** Sends wait while any player has this many unacknowledged reliable messages on the depot's channel (limit 511). */
	constexpr int32 QueueSoftLimit = RELIABLE_BUFFER / 2;
	/** Seconds between two summary log lines. */
	constexpr double SummaryIntervalSeconds = 60.0;

	enum class EHookState : uint8
	{
		NotStarted,
		Editor,
		DeferredToDepotNetworkFix,
		Installed,
	};

	struct FCounters
	{
		int64 Received = 0;        // messages the game tried to send while batching
		int64 Sent = 0;            // merged messages sent by a flush
		int64 Skipped = 0;         // merged amounts equal to the last one sent
		int64 Direct = 0;          // messages sent unbatched (setting off, or no player connected)
		int64 HeldBackFlushes = 0; // flushes that waited for a player's queue
		int32 HighestQueue = 0;
		int32 LargestSend = 0;

		void Merge(const FCounters& Other)
		{
			Received += Other.Received;
			Sent += Other.Sent;
			Skipped += Other.Skipped;
			Direct += Other.Direct;
			HeldBackFlushes += Other.HeldBackFlushes;
			HighestQueue = FMath::Max(HighestQueue, Other.HighestQueue);
			LargestSend = FMath::Max(LargestSend, Other.LargestSend);
		}
	};

	struct FBatchState
	{
		TWeakObjectPtr<AFGCentralStorageSubsystem> Subsystem;
		/** Latest unsent amount per item. */
		TMap<TWeakObjectPtr<UClass>, int32> Pending;
		/** Items in first-changed order; may hold items no longer in Pending, which a flush skips. */
		TArray<TWeakObjectPtr<UClass>> PendingOrder;
		/** Last amount players were sent per item, by a flush or directly. */
		TMap<TWeakObjectPtr<UClass>, int32> LastSent;
		double BoundTime = 0.0;
		double LastFlushTime = 0.0;
		double LastSummaryTime = 0.0;
		bool bHeldBack = false;
		FCounters Window; // since the last summary line
		FCounters Total;  // since this world's depot was bound, excluding Window
	};

	FBatchState GState;
	EHookState GHookState = EHookState::NotStarted;
	bool GEnabled = true;
	bool GSendingOwnFlush = false;
	FDelegateHandle GHookHandle;
	FTSTicker::FDelegateHandle GTickerHandle;

	/** True for the depot subsystem on a host or dedicated server (where other players can be connected). */
	bool IsMultiplayerAuthority(const AFGCentralStorageSubsystem* Self)
	{
		if (!Self || !Self->HasAuthority())
		{
			return false;
		}
		const UWorld* World = Self->GetWorld();
		const ENetMode Mode = World ? World->GetNetMode() : NM_Standalone;
		return Mode == NM_DedicatedServer || Mode == NM_ListenServer;
	}

	bool HasConnectedPlayers(const AFGCentralStorageSubsystem* Self)
	{
		const UWorld* World = Self->GetWorld();
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		return Driver && Driver->ClientConnections.Num() > 0;
	}

	void Bind(AFGCentralStorageSubsystem* Self)
	{
		if (GState.Subsystem.Get() == Self)
		{
			return;
		}
		if (GState.Pending.Num() > 0)
		{
			UE_LOG(LogEasyDDConfig, Warning, TEXT("Network batching: dropped %d unsent depot count updates of the previous world"), GState.Pending.Num());
		}
		GState = FBatchState();
		GState.Subsystem = Self;
		GState.BoundTime = FPlatformTime::Seconds();
		GState.LastFlushTime = GState.BoundTime;
		GState.LastSummaryTime = GState.BoundTime;
		UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching: merging depot count updates of %s, sending at most once per %.1f s, waiting while a player has %d+ unacknowledged messages"),
			*GetNameSafe(Self), FlushIntervalSeconds, QueueSoftLimit);
	}

	void Queue(AFGCentralStorageSubsystem* Self, UClass* ItemClass, const int32 NewAmount)
	{
		Bind(Self);
		++GState.Window.Received;
		const TWeakObjectPtr<UClass> Key(ItemClass);
		if (int32* Existing = GState.Pending.Find(Key))
		{
			*Existing = NewAmount;
			return;
		}
		GState.Pending.Add(Key, NewAmount);
		GState.PendingOrder.Add(Key);
	}

	/** The game is sending this message itself. It is newer than any pending amount of the item, and players now have it. */
	void NoteDirectSend(const AFGCentralStorageSubsystem* Self, UClass* ItemClass, const int32 NewAmount)
	{
		if (GState.Subsystem.Get() != Self)
		{
			return; // nothing was batched in this world yet
		}
		const TWeakObjectPtr<UClass> Key(ItemClass);
		GState.Pending.Remove(Key);
		GState.LastSent.Add(Key, NewAmount);
		++GState.Window.Direct;
	}

	/** Highest number of unacknowledged reliable messages on the depot's channel over all players. */
	int32 HighestPlayerQueue(AFGCentralStorageSubsystem* Subsystem)
	{
		const UWorld* World = Subsystem->GetWorld();
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		if (!Driver)
		{
			return 0;
		}
		int32 Highest = 0;
		for (UNetConnection* Connection : Driver->ClientConnections)
		{
			if (!Connection)
			{
				continue;
			}
			if (const UActorChannel* Channel = Connection->FindActorChannelRef(Subsystem))
			{
				Highest = FMath::Max(Highest, Channel->NumOutRec);
			}
		}
		return Highest;
	}

	void Flush(AFGCentralStorageSubsystem* Subsystem)
	{
		const int32 PlayerQueue = HighestPlayerQueue(Subsystem);
		GState.Window.HighestQueue = FMath::Max(GState.Window.HighestQueue, PlayerQueue);
		const int32 Budget = QueueSoftLimit - PlayerQueue;
		if (Budget <= 0)
		{
			++GState.Window.HeldBackFlushes;
			if (!GState.bHeldBack)
			{
				GState.bHeldBack = true;
				UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching: holding %d depot count updates back; a player has %d unacknowledged messages"),
					GState.Pending.Num(), PlayerQueue);
			}
			return;
		}
		if (GState.bHeldBack)
		{
			GState.bHeldBack = false;
			UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching: sending again (player queue %d)"), PlayerQueue);
		}

		TGuardValue<bool> OwnFlush(GSendingOwnFlush, true);
		int32 SentNow = 0;
		int32 Consumed = 0;
		while (Consumed < GState.PendingOrder.Num() && SentNow < Budget)
		{
			const TWeakObjectPtr<UClass> Key = GState.PendingOrder[Consumed++];
			int32 Amount = 0;
			if (!GState.Pending.RemoveAndCopyValue(Key, Amount))
			{
				continue; // already sent directly
			}
			UClass* ItemClass = Key.Get();
			if (!ItemClass)
			{
				continue;
			}
			const int32* Previous = GState.LastSent.Find(Key);
			if (Previous && *Previous == Amount)
			{
				++GState.Window.Skipped;
				continue;
			}
			(Subsystem->*AmountRpc)(TSubclassOf<UFGItemDescriptor>(ItemClass), Amount);
			GState.LastSent.Add(Key, Amount);
			++SentNow;
		}
		GState.PendingOrder.RemoveAt(0, Consumed, EAllowShrinking::No);
		GState.Window.Sent += SentNow;
		GState.Window.LargestSend = FMath::Max(GState.Window.LargestSend, SentNow);
		UE_LOG(LogEasyDDConfig, Verbose, TEXT("Network batching: sent %d (player queue %d, %d waiting)"), SentNow, PlayerQueue, GState.Pending.Num());
	}

	void LogSummary(const double Now)
	{
		if (Now - GState.LastSummaryTime < SummaryIntervalSeconds)
		{
			return;
		}
		const FCounters& W = GState.Window;
		if (W.Received > 0 || W.Direct > 0)
		{
			UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching, last %.0f s: %lld depot count updates merged into %lld sends (largest %d), %lld unchanged skipped, %lld sent directly, %lld sends held back, highest player queue %d of %d"),
				Now - GState.LastSummaryTime, W.Received, W.Sent, W.LargestSend, W.Skipped, W.Direct, W.HeldBackFlushes, W.HighestQueue, RELIABLE_BUFFER - 1);
		}
		GState.Total.Merge(W);
		GState.Window = FCounters();
		GState.LastSummaryTime = Now;
	}

	bool Tick(float)
	{
		AFGCentralStorageSubsystem* Subsystem = GState.Subsystem.Get();
		if (!Subsystem)
		{
			GState.Pending.Reset();
			GState.PendingOrder.Reset();
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		// Pending updates are flushed even after batching was turned off or the last player left, so none get stuck.
		if (GState.PendingOrder.Num() > 0 && Now - GState.LastFlushTime >= FlushIntervalSeconds)
		{
			GState.LastFlushTime = Now;
			Flush(Subsystem);
		}
		LogSummary(Now);
		return true;
	}

	void OnAmountRpc(TCallScope<void(*)(AFGCentralStorageSubsystem*, TSubclassOf<UFGItemDescriptor>, int32)>& Scope,
		AFGCentralStorageSubsystem* Self, TSubclassOf<UFGItemDescriptor> ItemClass, int32 NewAmount)
	{
		if (GSendingOwnFlush || !IsMultiplayerAuthority(Self))
		{
			return;
		}
		if (!GEnabled || !HasConnectedPlayers(Self))
		{
			NoteDirectSend(Self, ItemClass.Get(), NewAmount);
			return;
		}
		Queue(Self, ItemClass.Get(), NewAmount);
		Scope.Cancel();
	}
}

namespace EasyDD::NetBatch
{
	void Start()
	{
		if (GHookState != EHookState::NotStarted)
		{
			return;
		}
		if (WITH_EDITOR)
		{
			GHookState = EHookState::Editor;
			return;
		}
		if (IPluginManager::Get().FindEnabledPlugin(DepotNetworkFixPlugin).IsValid())
		{
			GHookState = EHookState::DeferredToDepotNetworkFix;
			UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching: Depot Network Fix is installed and does the same job, so Easy DD Config leaves depot messages to it"));
			return;
		}
		FAmountRpcHook::InstallHook(AmountRpcName);
		GHookHandle = FAmountRpcHook::AddHandlerBefore(&OnAmountRpc);
		GTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), TickerIntervalSeconds);
		GHookState = EHookState::Installed;
		UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching: hooked %s"), AmountRpcName);
	}

	void Stop()
	{
		if (GTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GTickerHandle);
			GTickerHandle.Reset();
		}
		if (GHookHandle.IsValid())
		{
			FAmountRpcHook::RemoveHandler(AmountRpcName, GHookHandle);
			GHookHandle.Reset();
		}
		GState = FBatchState();
		GHookState = EHookState::NotStarted;
	}

	void SetEnabled(const bool bEnabled)
	{
		if (GEnabled == bEnabled)
		{
			return;
		}
		GEnabled = bEnabled;
		if (GHookState == EHookState::Installed)
		{
			UE_LOG(LogEasyDDConfig, Log, TEXT("Network batching turned %s"), bEnabled ? TEXT("on") : TEXT("off (depot count updates are sent as in the base game)"));
		}
	}

	bool IsEnabled()
	{
		return GEnabled;
	}

	void DescribeStatus(const UWorld* World, TArray<FString>& OutLines)
	{
		switch (GHookState)
		{
		case EHookState::NotStarted:
			OutLines.Add(TEXT("  Network batching: not started"));
			return;
		case EHookState::Editor:
			OutLines.Add(TEXT("  Network batching: not used in the editor"));
			return;
		case EHookState::DeferredToDepotNetworkFix:
			OutLines.Add(TEXT("  Network batching: left to Depot Network Fix, which is installed"));
			return;
		case EHookState::Installed:
			break;
		}

		const ENetMode Mode = World ? World->GetNetMode() : NM_Standalone;
		if (Mode == NM_Client)
		{
			OutLines.Add(TEXT("  Network batching: done by the host or server"));
			return;
		}
		if (Mode == NM_Standalone)
		{
			OutLines.Add(TEXT("  Network batching: not needed in single player"));
			return;
		}
		OutLines.Add(FString::Printf(TEXT("  Network batching: %s; merged depot count updates are sent at most once per %.1f s, waiting while a player has %d+ unacknowledged messages"),
			GEnabled ? TEXT("on") : TEXT("off"), FlushIntervalSeconds, QueueSoftLimit));
		if (GState.Subsystem.IsValid())
		{
			FCounters All = GState.Total;
			All.Merge(GState.Window);
			OutLines.Add(FString::Printf(TEXT("    Last %.0f s: %lld updates merged into %lld sends (largest %d), %lld unchanged skipped, %lld sent directly, %lld sends held back, highest player queue %d of %d, %d waiting now"),
				FPlatformTime::Seconds() - GState.BoundTime, All.Received, All.Sent, All.LargestSend, All.Skipped, All.Direct, All.HeldBackFlushes,
				All.HighestQueue, RELIABLE_BUFFER - 1, GState.Pending.Num()));
		}
		else
		{
			OutLines.Add(TEXT("    No depot count update has been merged in this world yet."));
		}
	}
}
