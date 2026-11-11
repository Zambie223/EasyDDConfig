#pragma once

#include "CoreMinimal.h"
#include "EasyDDAdminSync.h"
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

	/** Sets one config value (a number, or true/false for switches) on this machine, persists it, and lets the normal change handler apply it. */
	static bool SetConfigValue(const UWorld* World, const FString& Key, const FString& ValueText, FString& OutMessage);

	/** Re-reads this mod's config file only and pushes its values like SetConfigValue does. */
	static bool ReloadConfigFromDisk(const UWorld* World, FString& OutMessage);

	/** Largest stack size of any solid item (the depot holds only solids); bounds extreme stack totals (see MaxSafeTotalStacks). */
	static int32 GetLargestUploadableStackSize();

	/** Reads this machine's AllowAdminMenuChanges switch (true if missing). Only a dedicated server's value matters. */
	static bool ReadAllowAdminMenuChanges(const UWorld* World);

	/** Writes vanilla, configured, written and effective values to the output device. */
	void DescribeStatus(FOutputDevice& Ar) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client: the server refused an admin menu change, so put the server's values back into the menu. */
	void OnAdminRequestRefused();

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
	/** Reads the BatchDepotNetworkUpdates switch (true if missing). */
	bool ReadBatchingSetting() const;
	/** Reads the AllowExtremeMultipliers switch (false if missing). */
	bool ReadExtremeSetting() const;
	/** Hands the batching switch to the network batcher and the extreme switch to the upload top-up. */
	void ApplySwitchSettings() const;
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

	/** Copies this server's own config values into the replicated ServerConfig* fields (for admins' menus). */
	void UpdateReplicatedConfig();

	// Client side
	UFUNCTION()
	void OnRep_AppliedValues();

	void ApplyReplicatedValuesLocally();

	// Client side, dedicated server admins: the menu shows and changes the server's values while connected.
	UFUNCTION()
	void OnRep_ServerConfig();
	void OnAdminCheck();
	bool IsLocalPlayerServerAdmin() const;
	void StartMirroring();
	void StopMirroring(const TCHAR* Why);
	void ApplyMirror();
	UFUNCTION()
	void OnClientMenuChanged();
	void SendMenuChanges();
	EasyDD::AdminSync::FValues GetReplicatedServerValues() const;

	/** The server's own config values (raw, before clamping), replicated so a server admin's menu can show them. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	float ServerConfigUpload = 1.0f;
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	float ServerConfigStack = 1.0f;
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	bool bServerConfigBatching = true;
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	bool bServerConfigExtreme = false;
	/** True on a dedicated server whose AllowAdminMenuChanges is on. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	bool bServerAllowsAdminMenu = false;
	/** True once the server filled the ServerConfig* fields. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerConfig)
	bool bServerConfigValid = false;

	bool bMirroring = false;
	/** Set while this machine writes the server's values into its own menu, so that write isn't sent back. */
	bool bApplyingMirror = false;
	EasyDD::AdminSync::FValues MirroredServerValues;
	TWeakObjectPtr<UConfigPropertySection> ClientBoundSection;
	FTimerHandle AdminCheckTimer;
	FTimerHandle ClientMenuDebounceTimer;

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
