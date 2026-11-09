#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "EasyDDConfigSubsystem.generated.h"

class AFGUnlockSubsystem;
class UConfigPropertySection;

/**
 * Applies the Easy DD Config multipliers to the Dimensional Depot.
 *
 * The server (or single player / listen host) reads its own Mod Options, writes the derived values into the
 * UFGCentralStorageSettings defaults, and asks the game to recalculate. The written values are replicated so
 * clients copy the host's numbers into their own settings, because clients compute upload time locally for UI.
 * Vanilla values are restored when the world ends.
 */
UCLASS()
class EASYDDCONFIG_API AEasyDDConfigSubsystem : public AModSubsystem
{
	GENERATED_BODY()

public:
	AEasyDDConfigSubsystem();

	/** Returns the subsystem in this world, or null (main menu, or on a client before replication). */
	static AEasyDDConfigSubsystem* Get(UWorld* World);

	/** Returns this mod's live configuration root section, or null if the configuration is not registered. */
	static UConfigPropertySection* GetConfigRootSection(const UWorld* World);

	/** Sets one config value on this machine, persists it, and lets the normal change handler apply it. */
	static bool SetConfigValue(const UWorld* World, const FString& Key, float NewValue, FString& OutMessage);

	/** Re-reads this mod's config file only and pushes its values like SetConfigValue does. */
	static bool ReloadConfigFromDisk(const UWorld* World, FString& OutMessage);

	/** Writes vanilla, configured, written and effective values to the output device. */
	void DescribeStatus(FOutputDevice& Ar) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void Init() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Server side
	void ApplyServer(const TCHAR* Reason);
	void RefreshGameObjects() const;
	/** Reads this machine's config and returns sanitized multipliers (rounded to one decimal place and clamped). */
	void ReadConfig(float& OutUploadMultiplier, float& OutStackMultiplier, bool bLogWarnings = true) const;
	void BindServerDelegates();
	void UnbindServerDelegates();
	void OnNextTickAfterBeginPlay();
	void OnDebounceElapsed();
	void OnWatchdog();
	void OnClientStatusSnapshot() const;

	UFUNCTION()
	void OnConfigValueChanged();

	UFUNCTION()
	void OnStackLimitResearched();

	// Client side
	UFUNCTION()
	void OnRep_AppliedValues();

	void ApplyReplicatedValuesLocally();

	/** UFGCentralStorageSettings::mDefaultTimeToUpload written by the server. 0 until the server applied. */
	UPROPERTY(ReplicatedUsing = OnRep_AppliedValues)
	float AppliedDefaultTimeToUpload = 0.0f;

	/** UFGCentralStorageSettings::mDefaultItemStackLimit written by the server. 0 until the server applied. */
	UPROPERTY(ReplicatedUsing = OnRep_AppliedValues)
	int32 AppliedDefaultItemStackLimit = 0;

	/** Sanitized multipliers in effect on the server, for display. */
	UPROPERTY(ReplicatedUsing = OnRep_AppliedValues)
	float AppliedUploadMultiplier = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_AppliedValues)
	float AppliedStackMultiplier = 1.0f;

	/** Research bonus (stacks) seen at the last server apply, for display. */
	UPROPERTY(Replicated)
	int32 AppliedResearchStackBonus = 0;

	TWeakObjectPtr<UConfigPropertySection> BoundConfigSection;
	TWeakObjectPtr<AFGUnlockSubsystem> BoundUnlockSubsystem;

	FTimerHandle DebounceTimer;
	FTimerHandle WatchdogTimer;
	FTimerHandle ClientStatusTimer;

	/** True once this instance wrote to the settings defaults, so EndPlay knows to restore them. */
	bool bWroteSettings = false;
};
