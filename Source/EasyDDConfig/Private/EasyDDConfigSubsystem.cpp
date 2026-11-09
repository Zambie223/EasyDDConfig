#include "EasyDDConfigSubsystem.h"

#include "EasyDDConfigConstants.h"
#include "EasyDDConfigLog.h"
#include "EasyDDConfigMath.h"

#include "Buildables/FGCentralStorageContainer.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/ModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FGCentralStorageSettings.h"
#include "FGCentralStorageSubsystem.h"
#include "FGUnlockSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystem/SubsystemActorManager.h"
#include "TimerManager.h"

namespace
{
	using namespace EasyDD::Math;

	/** Debounce for Mod Options changes; sliders and spinboxes fire once per step. */
	constexpr float ConfigChangeDebounceSeconds = 0.25f;

	/** How often the server checks that nothing else overwrote the settings defaults. */
	constexpr float WatchdogIntervalSeconds = 5.0f;

	/** Delay before a client writes one status snapshot to its log (lets replication settle). */
	constexpr float ClientStatusDelaySeconds = 10.0f;

	/** The game's own values, captured once per process before this mod writes anything. */
	struct FVanillaState
	{
		bool bCaptured = false;
		FVanillaValues Values;
	};
	FVanillaState GVanilla;

	UFGCentralStorageSettings* GetSettings()
	{
		return GetMutableDefault<UFGCentralStorageSettings>();
	}

	void CaptureVanillaOnce()
	{
		if (GVanilla.bCaptured)
		{
			return;
		}
		const UFGCentralStorageSettings* Settings = GetSettings();
		GVanilla.Values.TimeToUpload = Settings->mDefaultTimeToUpload;
		GVanilla.Values.MaxTimeToUploadPercentageDecrease = Settings->mMaxTimeToUploadPercentageDecrease;
		GVanilla.Values.ItemStackLimit = Settings->mDefaultItemStackLimit;
		GVanilla.bCaptured = true;

		if (GVanilla.Values.IsValid())
		{
			UE_LOG(LogEasyDDConfig, Log, TEXT("Captured vanilla depot settings: %.4f s/item (%.2f items/min), max research decrease %.2f%%, %d stack(s) base"),
				GVanilla.Values.TimeToUpload, ItemsPerMinute(GVanilla.Values.TimeToUpload),
				GVanilla.Values.MaxTimeToUploadPercentageDecrease, GVanilla.Values.ItemStackLimit);
		}
		else
		{
			UE_LOG(LogEasyDDConfig, Error, TEXT("Vanilla depot settings look invalid (%.4f s/item, %.2f%%, %d stacks). Easy DD Config will not change anything."),
				GVanilla.Values.TimeToUpload, GVanilla.Values.MaxTimeToUploadPercentageDecrease, GVanilla.Values.ItemStackLimit);
		}
	}

	void RestoreVanilla()
	{
		if (!GVanilla.bCaptured || !GVanilla.Values.IsValid())
		{
			return;
		}
		UFGCentralStorageSettings* Settings = GetSettings();
		Settings->mDefaultTimeToUpload = GVanilla.Values.TimeToUpload;
		Settings->mDefaultItemStackLimit = GVanilla.Values.ItemStackLimit;
		// mMaxTimeToUploadPercentageDecrease is never written by this mod.
	}

	const TCHAR* NetModeName(const UWorld* World)
	{
		if (!World)
		{
			return TEXT("no world");
		}
		switch (World->GetNetMode())
		{
		case NM_Standalone: return TEXT("single player");
		case NM_DedicatedServer: return TEXT("dedicated server");
		case NM_ListenServer: return TEXT("listen server (host)");
		case NM_Client: return TEXT("client");
		default: return TEXT("unknown");
		}
	}

	UConfigPropertyFloat* FindFloatProperty(UConfigPropertySection* Section, const FString& Key, FString* OutCanonicalKey = nullptr)
	{
		if (!Section)
		{
			return nullptr;
		}
		for (const TPair<FString, TObjectPtr<UConfigProperty>>& Pair : Section->SectionProperties)
		{
			if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase))
			{
				if (OutCanonicalKey)
				{
					*OutCanonicalKey = Pair.Key;
				}
				return Cast<UConfigPropertyFloat>(Pair.Value);
			}
		}
		return nullptr;
	}

	bool IsKnownKey(const FString& Key)
	{
		return Key.Equals(EasyDD::KeyUploadSpeedMultiplier, ESearchCase::IgnoreCase)
			|| Key.Equals(EasyDD::KeyStackLimitMultiplier, ESearchCase::IgnoreCase);
	}
}

AEasyDDConfigSubsystem::AEasyDDConfigSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);
}

AEasyDDConfigSubsystem* AEasyDDConfigSubsystem::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	return Manager ? Manager->GetSubsystemActor<AEasyDDConfigSubsystem>() : nullptr;
}

UConfigPropertySection* AEasyDDConfigSubsystem::GetConfigRootSection(const UWorld* World)
{
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UConfigManager* ConfigManager = GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	if (!ConfigManager)
	{
		return nullptr;
	}
	FConfigId ConfigId;
	ConfigId.ModReference = EasyDD::ModReference;
	return ConfigManager->GetConfigurationRootSection(ConfigId);
}

void AEasyDDConfigSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// REPNOTIFY_Always so a client also applies values equal to the class defaults on the initial bunch.
	DOREPLIFETIME_CONDITION_NOTIFY(AEasyDDConfigSubsystem, AppliedDefaultTimeToUpload, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AEasyDDConfigSubsystem, AppliedDefaultItemStackLimit, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AEasyDDConfigSubsystem, AppliedUploadMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AEasyDDConfigSubsystem, AppliedStackMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AEasyDDConfigSubsystem, AppliedResearchStackBonus);
}

void AEasyDDConfigSubsystem::Init()
{
	Super::Init();

	// On the server SML calls Init() at spawn: after the save is loaded and the game's subsystems exist,
	// but before any actor's BeginPlay, so vanilla BeginPlay code already computes with our values.
	// On clients Init() runs from BeginPlay; they are handled in BeginPlay/OnRep instead.
	if (!HasAuthority())
	{
		return;
	}
	int32 UploadersInWorld = 0;
	for (TActorIterator<AFGCentralStorageContainer> It(GetWorld()); It; ++It)
	{
		++UploadersInWorld;
	}
	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(GetWorld());
	UE_LOG(LogEasyDDConfig, Log, TEXT("Init on %s (uploaders in world: %d, registered with central storage: %d)"), NetModeName(GetWorld()),
		UploadersInWorld, CentralStorage ? CentralStorage->GetCentralStorageContainers().Num() : -1);
	CaptureVanillaOnce();
	BindServerDelegates();
	ApplyServer(TEXT("Init"));
}

void AEasyDDConfigSubsystem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		FTimerManager& Timers = GetWorldTimerManager();
		Timers.SetTimerForNextTick(this, &AEasyDDConfigSubsystem::OnNextTickAfterBeginPlay);
		Timers.SetTimer(WatchdogTimer, this, &AEasyDDConfigSubsystem::OnWatchdog, WatchdogIntervalSeconds, true);
	}
	else
	{
		UE_LOG(LogEasyDDConfig, Log, TEXT("BeginPlay on %s; using the host's values"), NetModeName(GetWorld()));
		ApplyReplicatedValuesLocally();
		// Clients have no console by default, so put one status snapshot in the client log for support.
		GetWorldTimerManager().SetTimer(ClientStatusTimer, this, &AEasyDDConfigSubsystem::OnClientStatusSnapshot, ClientStatusDelaySeconds, false);
	}
}

void AEasyDDConfigSubsystem::OnClientStatusSnapshot() const
{
	FOutputDeviceNull Discard; // DescribeStatus also writes every line to LogEasyDDConfig.
	DescribeStatus(Discard);
}

void AEasyDDConfigSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	UnbindServerDelegates();
	if (bWroteSettings)
	{
		RestoreVanilla();
		bWroteSettings = false;
		UE_LOG(LogEasyDDConfig, Log, TEXT("World ending; restored vanilla depot settings"));
	}
	Super::EndPlay(EndPlayReason);
}

void AEasyDDConfigSubsystem::OnNextTickAfterBeginPlay()
{
	// Everything has begun play now. Re-bind in case the unlock subsystem was not available at Init, then apply
	// again so existing containers and the central storage subsystem pick the values up.
	BindServerDelegates();
	ApplyServer(TEXT("PostBeginPlay"));
}

void AEasyDDConfigSubsystem::ReadConfig(float& OutUploadMultiplier, float& OutStackMultiplier, const bool bLogWarnings) const
{
	OutUploadMultiplier = DefaultMultiplier;
	OutStackMultiplier = DefaultMultiplier;

	UConfigPropertySection* Root = GetConfigRootSection(GetWorld());
	if (!Root)
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("Configuration '%s' is not registered; using vanilla multipliers (1.0)"), EasyDD::ModReference);
		return;
	}

	if (const UConfigPropertyFloat* Upload = FindFloatProperty(Root, EasyDD::KeyUploadSpeedMultiplier))
	{
		OutUploadMultiplier = Upload->Value;
	}
	else
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("Config key '%s' missing or not a float; using 1.0"), EasyDD::KeyUploadSpeedMultiplier);
	}

	if (const UConfigPropertyFloat* Stack = FindFloatProperty(Root, EasyDD::KeyStackLimitMultiplier))
	{
		OutStackMultiplier = Stack->Value;
	}
	else
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("Config key '%s' missing or not a float; using 1.0"), EasyDD::KeyStackLimitMultiplier);
	}

	const float RawUpload = OutUploadMultiplier;
	const float RawStack = OutStackMultiplier;
	// Sanitizing also rounds to one decimal place; only clamping is worth a warning.
	OutUploadMultiplier = SanitizeMultiplier(RawUpload, MinUploadMultiplier, MaxUploadMultiplier);
	OutStackMultiplier = SanitizeMultiplier(RawStack, MinStackMultiplier, MaxStackMultiplier);
	if (bLogWarnings && IsOutsideRange(RawUpload, MinUploadMultiplier, MaxUploadMultiplier))
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("%s = %f is outside %.1f-%.1f; using %.1f"), EasyDD::KeyUploadSpeedMultiplier, RawUpload, MinUploadMultiplier, MaxUploadMultiplier, OutUploadMultiplier);
	}
	if (bLogWarnings && IsOutsideRange(RawStack, MinStackMultiplier, MaxStackMultiplier))
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("%s = %f is outside %.1f-%.1f; using %.1f"), EasyDD::KeyStackLimitMultiplier, RawStack, MinStackMultiplier, MaxStackMultiplier, OutStackMultiplier);
	}
}

void AEasyDDConfigSubsystem::ApplyServer(const TCHAR* Reason)
{
	if (!HasAuthority())
	{
		return;
	}
	CaptureVanillaOnce();
	if (!GVanilla.Values.IsValid())
	{
		return;
	}

	float UploadMultiplier = DefaultMultiplier;
	float StackMultiplier = DefaultMultiplier;
	ReadConfig(UploadMultiplier, StackMultiplier);

	UFGCentralStorageSettings* Settings = GetSettings();
	Settings->mDefaultTimeToUpload = ComputeDefaultTimeToUpload(GVanilla.Values.TimeToUpload, UploadMultiplier);

	// Research adds stacks on top of mDefaultItemStackLimit, so derive the bonus from the game's own getter.
	int32 ResearchBonus = 0;
	if (const AFGUnlockSubsystem* Unlock = AFGUnlockSubsystem::Get(GetWorld()))
	{
		ResearchBonus = Unlock->GetCentralStorageItemStackLimit() - Settings->mDefaultItemStackLimit;
		if (ResearchBonus < 0)
		{
			UE_LOG(LogEasyDDConfig, Warning, TEXT("Research stack bonus came out negative (%d); treating it as 0. The game's stack formula may have changed."), ResearchBonus);
			ResearchBonus = 0;
		}
	}
	Settings->mDefaultItemStackLimit = ComputeDefaultItemStackLimit(GVanilla.Values.ItemStackLimit, ResearchBonus, StackMultiplier);
	bWroteSettings = true;

	AppliedDefaultTimeToUpload = Settings->mDefaultTimeToUpload;
	AppliedDefaultItemStackLimit = Settings->mDefaultItemStackLimit;
	AppliedUploadMultiplier = UploadMultiplier;
	AppliedStackMultiplier = StackMultiplier;
	AppliedResearchStackBonus = ResearchBonus;
	ForceNetUpdate();

	RefreshGameObjects();

	UE_LOG(LogEasyDDConfig, Log, TEXT("Applied (%s): upload x%.2f -> base %.4f s/item (%.2f items/min before research); stacks x%.2f -> base %d + research %d = %d"),
		Reason, UploadMultiplier, Settings->mDefaultTimeToUpload, ItemsPerMinute(Settings->mDefaultTimeToUpload),
		StackMultiplier, Settings->mDefaultItemStackLimit, ResearchBonus, Settings->mDefaultItemStackLimit + ResearchBonus);
}

void AEasyDDConfigSubsystem::RefreshGameObjects() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World))
	{
		if (CentralStorage->HasActorBegunPlay())
		{
			CentralStorage->UpdateItemStackLimit();
		}
	}
	// Iterate the world rather than GetCentralStorageContainers(): containers register in BeginPlay, so that list can be empty early.
	for (TActorIterator<AFGCentralStorageContainer> It(World); It; ++It)
	{
		AFGCentralStorageContainer* Container = *It;
		if (Container && Container->HasActorBegunPlay() && !Container->IsActorBeingDestroyed())
		{
			Container->UpdateTimeToUpload();
		}
	}
}

void AEasyDDConfigSubsystem::BindServerDelegates()
{
	UWorld* World = GetWorld();

	UConfigPropertySection* Root = GetConfigRootSection(World);
	if (Root && BoundConfigSection.Get() != Root)
	{
		if (UConfigPropertySection* Old = BoundConfigSection.Get())
		{
			Old->OnPropertyValueChanged.RemoveDynamic(this, &AEasyDDConfigSubsystem::OnConfigValueChanged);
		}
		Root->OnPropertyValueChanged.AddUniqueDynamic(this, &AEasyDDConfigSubsystem::OnConfigValueChanged);
		BoundConfigSection = Root;
	}

	AFGUnlockSubsystem* Unlock = AFGUnlockSubsystem::Get(World);
	if (Unlock && BoundUnlockSubsystem.Get() != Unlock)
	{
		if (AFGUnlockSubsystem* Old = BoundUnlockSubsystem.Get())
		{
			Old->mOnCentralStorageItemStackLimitUnlocked.RemoveDynamic(this, &AEasyDDConfigSubsystem::OnStackLimitResearched);
		}
		Unlock->mOnCentralStorageItemStackLimitUnlocked.AddUniqueDynamic(this, &AEasyDDConfigSubsystem::OnStackLimitResearched);
		BoundUnlockSubsystem = Unlock;
	}
}

void AEasyDDConfigSubsystem::UnbindServerDelegates()
{
	if (UConfigPropertySection* Section = BoundConfigSection.Get())
	{
		Section->OnPropertyValueChanged.RemoveDynamic(this, &AEasyDDConfigSubsystem::OnConfigValueChanged);
	}
	BoundConfigSection.Reset();

	if (AFGUnlockSubsystem* Unlock = BoundUnlockSubsystem.Get())
	{
		Unlock->mOnCentralStorageItemStackLimitUnlocked.RemoveDynamic(this, &AEasyDDConfigSubsystem::OnStackLimitResearched);
	}
	BoundUnlockSubsystem.Reset();
}

void AEasyDDConfigSubsystem::OnConfigValueChanged()
{
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().SetTimer(DebounceTimer, this, &AEasyDDConfigSubsystem::OnDebounceElapsed, ConfigChangeDebounceSeconds, false);
}

void AEasyDDConfigSubsystem::OnDebounceElapsed()
{
	// Focusing or committing a Mod Options box fires a change event even when nothing changed, and a small drag can
	// land on the same rounded value. Re-applying would only repeat identical writes and refreshes, so skip it.
	// Anything else that changes the depot settings (research, re-registration, other mods) has its own apply path.
	if (bWroteSettings)
	{
		float Upload = DefaultMultiplier;
		float Stack = DefaultMultiplier;
		ReadConfig(Upload, Stack, /*bLogWarnings=*/false);
		if (Upload == AppliedUploadMultiplier && Stack == AppliedStackMultiplier)
		{
			UE_LOG(LogEasyDDConfig, Verbose, TEXT("Config change event with unchanged values (upload x%.1f, stacks x%.1f); nothing to apply"), Upload, Stack);
			return;
		}
	}
	ApplyServer(TEXT("Config changed"));
}

void AEasyDDConfigSubsystem::OnStackLimitResearched()
{
	ApplyServer(TEXT("Stack limit research"));
}

void AEasyDDConfigSubsystem::OnWatchdog()
{
	const UConfigPropertySection* PreviouslyBound = BoundConfigSection.Get();
	BindServerDelegates();
	if (BoundConfigSection.Get() != PreviouslyBound)
	{
		ApplyServer(TEXT("Config re-registered"));
		return;
	}
	if (!bWroteSettings)
	{
		return;
	}
	const UFGCentralStorageSettings* Settings = GetSettings();
	if (!FMath::IsNearlyEqual(Settings->mDefaultTimeToUpload, AppliedDefaultTimeToUpload) || Settings->mDefaultItemStackLimit != AppliedDefaultItemStackLimit)
	{
		UE_LOG(LogEasyDDConfig, Warning, TEXT("Depot settings were changed by something else (%.4f s/item, %d stacks); re-applying"),
			Settings->mDefaultTimeToUpload, Settings->mDefaultItemStackLimit);
		ApplyServer(TEXT("Watchdog"));
	}
}

void AEasyDDConfigSubsystem::OnRep_AppliedValues()
{
	ApplyReplicatedValuesLocally();
}

void AEasyDDConfigSubsystem::ApplyReplicatedValuesLocally()
{
	if (HasAuthority())
	{
		return;
	}
	if (!(FMath::IsFinite(AppliedDefaultTimeToUpload) && AppliedDefaultTimeToUpload > 0.0f) || AppliedDefaultItemStackLimit <= 0)
	{
		return; // The host has not applied yet.
	}
	CaptureVanillaOnce();
	UFGCentralStorageSettings* Settings = GetSettings();
	if (Settings->mDefaultTimeToUpload == AppliedDefaultTimeToUpload && Settings->mDefaultItemStackLimit == AppliedDefaultItemStackLimit && bWroteSettings)
	{
		return;
	}
	Settings->mDefaultTimeToUpload = AppliedDefaultTimeToUpload;
	Settings->mDefaultItemStackLimit = AppliedDefaultItemStackLimit;
	bWroteSettings = true;
	UE_LOG(LogEasyDDConfig, Log, TEXT("Using host values: upload x%.2f (base %.4f s/item), stacks x%.2f (base %d)"),
		AppliedUploadMultiplier, AppliedDefaultTimeToUpload, AppliedStackMultiplier, AppliedDefaultItemStackLimit);
}

bool AEasyDDConfigSubsystem::SetConfigValue(const UWorld* World, const FString& Key, const float NewValue, FString& OutMessage)
{
	if (World && World->GetNetMode() == NM_Client)
	{
		OutMessage = TEXT("Refused: this is a client. Settings come from the host or server.");
		return false;
	}
	if (!IsKnownKey(Key))
	{
		OutMessage = FString::Printf(TEXT("Unknown key '%s'. Valid keys: %s, %s"), *Key, EasyDD::KeyUploadSpeedMultiplier, EasyDD::KeyStackLimitMultiplier);
		return false;
	}
	if (!FMath::IsFinite(NewValue))
	{
		OutMessage = TEXT("Refused: value must be a finite number.");
		return false;
	}
	UConfigPropertySection* Root = GetConfigRootSection(World);
	FString CanonicalKey;
	UConfigPropertyFloat* Property = FindFloatProperty(Root, Key, &CanonicalKey);
	if (!Property)
	{
		OutMessage = FString::Printf(TEXT("Configuration '%s' or key '%s' not found."), EasyDD::ModReference, *Key);
		return false;
	}
	Property->Value = NewValue;
	// MarkDirty saves the file, refreshes SML's caches and fires OnPropertyValueChanged, which applies the value.
	Property->MarkDirty();

	const bool bUpload = CanonicalKey.Equals(EasyDD::KeyUploadSpeedMultiplier);
	const float Sanitized = bUpload
		? SanitizeMultiplier(NewValue, MinUploadMultiplier, MaxUploadMultiplier)
		: SanitizeMultiplier(NewValue, MinStackMultiplier, MaxStackMultiplier);
	const bool bClamped = bUpload
		? IsOutsideRange(NewValue, MinUploadMultiplier, MaxUploadMultiplier)
		: IsOutsideRange(NewValue, MinStackMultiplier, MaxStackMultiplier);
	FString Note;
	if (bClamped)
	{
		Note = FString::Printf(TEXT(" (will be clamped to %g)"), Sanitized);
	}
	else if (Sanitized != NewValue)
	{
		Note = FString::Printf(TEXT(" (applied as %g, rounded to one decimal place)"), Sanitized);
	}
	OutMessage = FString::Printf(TEXT("%s set to %g%s. Applies within a second."), *CanonicalKey, NewValue, *Note);
	return true;
}

bool AEasyDDConfigSubsystem::ReloadConfigFromDisk(const UWorld* World, FString& OutMessage)
{
	if (World && World->GetNetMode() == NM_Client)
	{
		OutMessage = TEXT("Refused: this is a client. Settings come from the host or server.");
		return false;
	}
	UConfigPropertySection* Root = GetConfigRootSection(World);
	if (!Root)
	{
		OutMessage = FString::Printf(TEXT("Configuration '%s' is not registered."), EasyDD::ModReference);
		return false;
	}

	const FString FilePath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(UConfigManager::GetConfigurationFolderPath(), FString(EasyDD::ModReference) + TEXT(".cfg")));
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *FilePath))
	{
		OutMessage = FString::Printf(TEXT("Could not read %s"), *FilePath);
		return false;
	}
	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Contents);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		OutMessage = FString::Printf(TEXT("%s is not valid JSON; nothing changed."), *FilePath);
		return false;
	}

	int32 Updated = 0;
	for (const TCHAR* Key : {EasyDD::KeyUploadSpeedMultiplier, EasyDD::KeyStackLimitMultiplier})
	{
		double Number = 0.0;
		UConfigPropertyFloat* Property = FindFloatProperty(Root, Key);
		if (Property && Json->TryGetNumberField(Key, Number) && FMath::IsFinite(Number))
		{
			Property->Value = static_cast<float>(Number);
			++Updated;
		}
	}
	if (Updated == 0)
	{
		OutMessage = FString::Printf(TEXT("No usable values found in %s; nothing changed."), *FilePath);
		return false;
	}
	// One MarkDirty on the root saves, refreshes caches and fires the change handler once.
	Root->MarkDirty();
	OutMessage = FString::Printf(TEXT("Reloaded %d value(s) from %s. Applies within a second."), Updated, *FilePath);
	return true;
}

void AEasyDDConfigSubsystem::DescribeStatus(FOutputDevice& Ar) const
{
	UWorld* World = GetWorld();
	const UFGCentralStorageSettings* Settings = GetSettings();
	TArray<FString> Lines;

	Lines.Add(FString::Printf(TEXT("Easy DD Config status (%s)"), NetModeName(World)));
	if (GVanilla.bCaptured)
	{
		Lines.Add(FString::Printf(TEXT("  Vanilla: %.4f s/item (%.2f items/min), max research decrease %.2f%%, %d stack(s) base%s"),
			GVanilla.Values.TimeToUpload, ItemsPerMinute(GVanilla.Values.TimeToUpload), GVanilla.Values.MaxTimeToUploadPercentageDecrease,
			GVanilla.Values.ItemStackLimit, GVanilla.Values.IsValid() ? TEXT("") : TEXT("  [INVALID - mod inactive]")));
	}
	else
	{
		Lines.Add(TEXT("  Vanilla: not captured yet (nothing applied)"));
	}

	if (HasAuthority())
	{
		float Upload = DefaultMultiplier;
		float Stack = DefaultMultiplier;
		ReadConfig(Upload, Stack);
		Lines.Add(FString::Printf(TEXT("  Config on this machine: %s=%.2f, %s=%.2f"), EasyDD::KeyUploadSpeedMultiplier, Upload, EasyDD::KeyStackLimitMultiplier, Stack));
	}
	else
	{
		Lines.Add(TEXT("  Config on this machine: ignored (client); the host's values apply"));
	}
	Lines.Add(FString::Printf(TEXT("  In effect: upload x%.2f, stacks x%.2f, research stack bonus %d"), AppliedUploadMultiplier, AppliedStackMultiplier, AppliedResearchStackBonus));
	Lines.Add(FString::Printf(TEXT("  Settings now: mDefaultTimeToUpload=%.4f, mMaxTimeToUploadPercentageDecrease=%.2f, mDefaultItemStackLimit=%d"),
		Settings->mDefaultTimeToUpload, Settings->mMaxTimeToUploadPercentageDecrease, Settings->mDefaultItemStackLimit));

	if (const AFGUnlockSubsystem* Unlock = AFGUnlockSubsystem::Get(World))
	{
		const float Time = Unlock->GetCentralStorageTimeToUpload();
		Lines.Add(FString::Printf(TEXT("  Game (unlock subsystem): %.4f s/item (%.2f items/min), %d stack(s)"),
			Time, ItemsPerMinute(Time), Unlock->GetCentralStorageItemStackLimit()));
	}
	if (const AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World))
	{
		const float Time = CentralStorage->GetCentralStorageTimeToUpload();
		Lines.Add(FString::Printf(TEXT("  Game (central storage): %.4f s/item (%.2f items/min), %d stack(s) per item"),
			Time, ItemsPerMinute(Time), CentralStorage->GetCentralStorageItemStackLimit()));
	}
	int32 Containers = 0;
	if (World)
	{
		for (TActorIterator<AFGCentralStorageContainer> It(World); It; ++It)
		{
			++Containers;
		}
	}
	Lines.Add(FString::Printf(TEXT("  Uploaders in world: %d"), Containers));

	for (const FString& Line : Lines)
	{
		Ar.Log(Line);
		UE_LOG(LogEasyDDConfig, Log, TEXT("%s"), *Line);
	}
}
