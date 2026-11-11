#pragma once

#include "CoreMinimal.h"

class UConfigManager;
class UConfigPropertySection;

/**
 * Helpers for letting a dedicated server's admin change the server's settings from their own Mods menu.
 *
 * While an admin is connected, their menu shows the server's values ("mirroring"). Their own values are saved to a
 * backup file first and written back when they leave, or on the next game start if the game ended before that.
 */
namespace EasyDD::AdminSync
{
	/** The settings an admin can change from the menu (not AllowAdminMenuChanges, which only the server's own cfg or console sets). */
	struct FValues
	{
		float Upload = 1.0f;
		float Stack = 1.0f;
		bool bBatching = true;
		bool bExtreme = false;
	};

	bool IsSyncedKey(const FString& Key);

	/** Reads the synced settings from a configuration root. False if a property is missing. */
	bool Read(UConfigPropertySection* Root, FValues& Out);

	/** Sets the synced properties' values (no save, no change event). True if any value changed. */
	bool Write(UConfigPropertySection* Root, const FValues& In);

	/** Keys whose value differs between From and To, with To's value as text (for EasyDD.Set's parser). */
	TArray<TPair<FString, FString>> Diff(const FValues& From, const FValues& To);

	FString BackupFilePath();
	bool HasBackup();
	bool SaveBackup(const FValues& Own);

	/**
	 * If a backup exists: writes it into the configuration, saves the config file at once and deletes the backup.
	 * True if values were restored.
	 */
	bool RestoreBackup(UConfigManager* ConfigManager, UConfigPropertySection* Root, const TCHAR* Why);
}
