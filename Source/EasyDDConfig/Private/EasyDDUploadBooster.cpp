#include "EasyDDUploadBooster.h"

#include "Buildables/FGCentralStorageContainer.h"
#include "EasyDDConfigLog.h"
#include "Engine/World.h"
#include "FGCentralStorageSubsystem.h"
#include "FGInventoryComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "Patching/NativeHookManager.h"
#include "UObject/UnrealType.h"

#include <atomic>

// How the base game uploads (seen in 0.3.0 testing; the game's source isn't available): an uploader's Factory_Tick only
// adds time to its timer, and the upload itself happens elsewhere, one item per call to the central storage subsystem's
// UploadItemFromInventoryToCentralStorage, at most one per uploader per frame. The uploader has no Tick of its own (a
// hook on it landed on a Tick shared with other buildings). So the top-up hooks that upload function: when the game
// uploads from an uploader's storage, the mod works out from game time how many more items that uploader is due and
// uploads them through the same function. The mod keeps its own time account per uploader and never touches the game's
// timer.
namespace
{
	using FUploadFn = bool (AFGCentralStorageSubsystem::*)(UFGInventoryComponent*, int32);
	using FUploadHook = HookInvoker<FUploadFn, &AFGCentralStorageSubsystem::UploadItemFromInventoryToCentralStorage>;
	constexpr const TCHAR* UploadFnName = TEXT("AFGCentralStorageSubsystem::UploadItemFromInventoryToCentralStorage");

	/**
	 * Most extra items one uploader uploads at once, with normal settings. Bounds the work per frame (660 uploaders x
	 * this many calls) and is far above what normal settings need (x20 with all research is 80 items/s, 4 per frame at 20
	 * frames per second). With "Allow extreme multipliers" on there is no such cap (the user's decision): an uploader then
	 * uploads everything it is owed, limited only by its contents and the depot's room.
	 */
	constexpr int32 MaxExtraUploadsPerTick = 100;
	std::atomic<bool> GExtremeMode{false};

	/**
	 * Time since an uploader's previous upload is counted in full only up to this many items' time (or frames, whichever
	 * is longer). A longer gap means the uploader was idle or blocked (empty, or the depot full), and that wait must not
	 * turn into a burst of uploads: it then starts fresh, with no extra upload.
	 */
	constexpr double MaxGapItems = 4.0;
	/** Account entries for uploaders that haven't uploaded for this long are dropped. */
	constexpr double ForgetAfterSeconds = 10.0;

	enum class EBoostHookState : uint8 { NotStarted, Editor, Installed, Failed };
	EBoostHookState GBoostHookState = EBoostHookState::NotStarted;
	FDelegateHandle GUploadHandle;

	/** On by default. The console variable exists to compare frame times with and without top-ups, and as a support switch. */
	TAutoConsoleVariable<int32> CVarUploadTopUp(
		TEXT("EasyDD.UploadTopUp"), 1,
		TEXT("1 (default): Dimensional Depot Uploaders upload every item they are due, even when that is more than one per frame ")
		TEXT("(high upload speeds, low frame or server tick rates). 0: the base game's limit of one item per uploader per frame."),
		ECVF_Default);

	FFloatProperty* GTimeToUploadProp = nullptr; // AFGCentralStorageContainer::mTimeToUpload: seconds per item, research applied

	/** True while the mod's own extra uploads run on this thread, so the hook doesn't count them as the game's. */
	thread_local bool TInTopUp = false;

	struct FUploaderAccount
	{
		double LastUploadTime = 0.0; // world time of the previous counted upload
		double Carry = 0.0;          // time already counted but not yet worth a whole item
	};
	FCriticalSection GAccountsLock;
	TMap<TWeakObjectPtr<AFGCentralStorageContainer>, FUploaderAccount> GAccounts;
	double GLastPruneTime = 0.0;

	std::atomic<int64> GGameUploadsSeen{0};
	std::atomic<int64> GExtraUploads{0};
	std::atomic<int64> GBoostedTicks{0};
	std::atomic<int64> GBlockedTicks{0};

	// Diagnostics (rate-limited, first occurrences only; EasyDD.TopUpDiag re-arms them).
	std::atomic<int32> GDiagSamples{0};
	std::atomic<int32> GDiagFailures{0};
	std::atomic<int32> GDiagSuccesses{0};
	constexpr int32 DiagSampleLimit = 12;
	constexpr int32 DiagFailureLimit = 8;
	constexpr int32 DiagSuccessLimit = 3;

	FAutoConsoleCommand GTopUpDiagCommand(
		TEXT("EasyDD.TopUpDiag"),
		TEXT("Logs the next few upload top-up decisions again (game uploads from uploaders, failures, successes) to the game log."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			GDiagSamples = 0;
			GDiagFailures = 0;
			GDiagSuccesses = 0;
			UE_LOG(LogEasyDDConfig, Log, TEXT("Upload top-up diag: re-armed"));
		}));

	int32 CountItems(const UFGInventoryComponent* Inventory)
	{
		int32 Total = 0;
		const int32 Slots = Inventory->GetSizeLinear();
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			FInventoryStack Stack;
			if (Inventory->GetStackFromIndex(Slot, Stack))
			{
				Total += Stack.NumItems;
			}
		}
		return Total;
	}

	/** Uploads one item from the first slot the depot accepts. False if nothing could be uploaded. */
	bool UploadOne(AFGCentralStorageSubsystem* Subsystem, UFGInventoryComponent* Inventory)
	{
		const int32 Slots = Inventory->GetSizeLinear();
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (Inventory->IsIndexEmpty(Slot))
			{
				continue;
			}
			if (Subsystem->UploadItemFromInventoryToCentralStorage(Inventory, Slot))
			{
				return true;
			}
		}
		return false;
	}

	/** How many extra items the uploader is due right after the game uploaded one, from the mod's time account. */
	int32 ExtraItemsDue(AFGCentralStorageContainer* Uploader, const double Now, const double FrameTime, const float TimeToUpload, double& OutGap)
	{
		FScopeLock Lock(&GAccountsLock);
		if (Now - GLastPruneTime > ForgetAfterSeconds)
		{
			GLastPruneTime = Now;
			for (auto It = GAccounts.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid() || Now - It.Value().LastUploadTime > ForgetAfterSeconds)
				{
					It.RemoveCurrent();
				}
			}
		}
		FUploaderAccount* Account = GAccounts.Find(Uploader);
		if (!Account)
		{
			GAccounts.Add(Uploader, FUploaderAccount{Now, 0.0});
			OutGap = -1.0;
			return 0; // first upload seen from this uploader: start its account
		}
		const double Gap = Now - Account->LastUploadTime;
		OutGap = Gap;
		Account->LastUploadTime = Now;
		if (Gap <= 0.0 || Gap > MaxGapItems * FMath::Max(static_cast<double>(TimeToUpload), FrameTime))
		{
			Account->Carry = 0.0; // a second upload in the same frame, or idle/blocked for a while: no extra, start fresh
			return 0;
		}
		// Time paid for since the previous upload, minus the item the game just uploaded. The carry may go slightly
		// negative (the game uploaded a little early) so that, at speeds the game handles on its own, early and late
		// uploads cancel out instead of slowly adding up to a spurious extra item.
		const double Owed = Account->Carry + Gap - TimeToUpload;
		const int64 Whole = static_cast<int64>(FMath::FloorToDouble(Owed / TimeToUpload));
		const int32 PerTickCap = GExtremeMode.load(std::memory_order_relaxed) ? MAX_int32 : MaxExtraUploadsPerTick;
		const int32 Due = static_cast<int32>(FMath::Clamp<int64>(Whole, 0, PerTickCap));
		Account->Carry = FMath::Clamp(Owed - Due * static_cast<double>(TimeToUpload), -static_cast<double>(TimeToUpload), static_cast<double>(TimeToUpload));
		return Due;
	}

	void SettleAccount(AFGCentralStorageContainer* Uploader, const int32 Due, const int32 Done)
	{
		if (Done >= Due)
		{
			return;
		}
		FScopeLock Lock(&GAccountsLock);
		if (FUploaderAccount* Account = GAccounts.Find(Uploader))
		{
			Account->Carry = 0.0; // ran out of items or depot room: keep no backlog, so nothing bursts out later
		}
	}

	void OnUploadItem(TCallScope<bool(*)(AFGCentralStorageSubsystem*, UFGInventoryComponent*, int32)>& Scope,
		AFGCentralStorageSubsystem* Subsystem, UFGInventoryComponent* Inventory, int32 SlotIndex)
	{
		if (TInTopUp)
		{
			return; // our own extra upload: let the game run it normally
		}
		AFGCentralStorageContainer* Uploader = Inventory ? Cast<AFGCentralStorageContainer>(Inventory->GetOwner()) : nullptr;
		if (!Subsystem || !Uploader || Inventory != Uploader->GetStorageInventory() || !Uploader->HasAuthority()
			|| CVarUploadTopUp.GetValueOnAnyThread() == 0)
		{
			return; // not an uploader's storage (e.g. a player's own upload slots), a client, or top-ups off
		}

		const int32 ItemsBefore = CountItems(Inventory);
		const bool bUploaded = Scope(Subsystem, Inventory, SlotIndex);
		Scope.Override(bUploaded);
		if (!bUploaded)
		{
			return;
		}
		GGameUploadsSeen.fetch_add(1, std::memory_order_relaxed);

		const float TimeToUpload = GTimeToUploadProp->GetPropertyValue_InContainer(Uploader);
		UWorld* World = Uploader->GetWorld();
		if (!(TimeToUpload > 0.0f) || !World)
		{
			return;
		}
		const double Now = World->GetTimeSeconds();
		const double FrameTime = World->GetDeltaSeconds();
		double Gap = 0.0;
		const int32 Due = ExtraItemsDue(Uploader, Now, FrameTime, TimeToUpload, Gap);
		if (GDiagSamples.fetch_add(1, std::memory_order_relaxed) < DiagSampleLimit)
		{
			UE_LOG(LogEasyDDConfig, Log, TEXT("Upload top-up diag: %s game upload, items %d -> %d, %.6f s since its previous upload, frame %.6f, time per item %.6f, extra due %d"),
				*Uploader->GetName(), ItemsBefore, CountItems(Inventory), Gap, FrameTime, TimeToUpload, Due);
		}
		if (Due <= 0)
		{
			return;
		}

		int32 Done = 0;
		{
			TGuardValue<bool> Guard(TInTopUp, true);
			while (Done < Due && UploadOne(Subsystem, Inventory))
			{
				++Done;
			}
		}
		SettleAccount(Uploader, Due, Done);
		if (Done < Due)
		{
			GBlockedTicks.fetch_add(1, std::memory_order_relaxed);
			if (GDiagFailures.fetch_add(1, std::memory_order_relaxed) < DiagFailureLimit)
			{
				UE_LOG(LogEasyDDConfig, Log, TEXT("Upload top-up diag: %s uploaded %d of %d extra due (%d items left)"), *Uploader->GetName(), Done, Due, CountItems(Inventory));
			}
		}
		else if (GDiagSuccesses.fetch_add(1, std::memory_order_relaxed) < DiagSuccessLimit)
		{
			UE_LOG(LogEasyDDConfig, Log, TEXT("Upload top-up diag: %s uploaded %d extra"), *Uploader->GetName(), Done);
		}
		if (Done > 0)
		{
			GExtraUploads.fetch_add(Done, std::memory_order_relaxed);
			GBoostedTicks.fetch_add(1, std::memory_order_relaxed);
		}
	}
}

namespace EasyDD::UploadBoost
{
	void Start()
	{
		if (GBoostHookState != EBoostHookState::NotStarted)
		{
			return;
		}
		if (WITH_EDITOR)
		{
			GBoostHookState = EBoostHookState::Editor;
			return;
		}
		GTimeToUploadProp = FindFProperty<FFloatProperty>(AFGCentralStorageContainer::StaticClass(), TEXT("mTimeToUpload"));
		if (!GTimeToUploadProp)
		{
			GBoostHookState = EBoostHookState::Failed;
			UE_LOG(LogEasyDDConfig, Warning, TEXT("Upload top-up unavailable: the uploader's upload time was not found (the game may have changed). Uploads stay at the game's limit of one item per frame."));
			return;
		}
		FUploadHook::InstallHook(UploadFnName);
		GUploadHandle = FUploadHook::AddHandlerBefore(&OnUploadItem);
		GBoostHookState = EBoostHookState::Installed;
		UE_LOG(LogEasyDDConfig, Log, TEXT("Upload top-up: hooked %s"), UploadFnName);
	}

	void Stop()
	{
		if (GUploadHandle.IsValid())
		{
			FUploadHook::RemoveHandler(UploadFnName, GUploadHandle);
			GUploadHandle.Reset();
		}
		{
			FScopeLock Lock(&GAccountsLock);
			GAccounts.Reset();
		}
		GBoostHookState = EBoostHookState::NotStarted;
	}

	void SetExtremeMode(const bool bExtreme)
	{
		GExtremeMode = bExtreme;
	}

	void DescribeStatus(TArray<FString>& OutLines)
	{
		switch (GBoostHookState)
		{
		case EBoostHookState::NotStarted: OutLines.Add(TEXT("  Upload top-up: not started")); return;
		case EBoostHookState::Editor: OutLines.Add(TEXT("  Upload top-up: not used in the editor")); return;
		case EBoostHookState::Failed: OutLines.Add(TEXT("  Upload top-up: unavailable (uploader upload time not found)")); return;
		case EBoostHookState::Installed: break;
		}
		const FString PerTick = GExtremeMode ? FString(TEXT("no per-frame cap (extreme multipliers)")) : FString::Printf(TEXT("at most %d extra per uploader at once"), MaxExtraUploadsPerTick);
		OutLines.Add(FString::Printf(TEXT("  Upload top-up: %s (EasyDD.UploadTopUp); game uploads seen %lld; %lld extra items uploaded in %lld top-ups, %lld stopped early (empty or depot full), %s"),
			CVarUploadTopUp.GetValueOnAnyThread() != 0 ? TEXT("on") : TEXT("off"), GGameUploadsSeen.load(), GExtraUploads.load(), GBoostedTicks.load(), GBlockedTicks.load(), *PerTick));
	}
}
