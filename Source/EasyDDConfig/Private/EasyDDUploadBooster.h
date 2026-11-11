#pragma once

#include "CoreMinimal.h"

/**
 * Upload top-up: lets Dimensional Depot Uploaders upload every item they are due, even more than one per tick.
 *
 * The game uploads at most one item per uploader per Factory_Tick, so an uploader tops out at one item per tick
 * whatever its upload time is: about 3,600 items/min at 60 frames per second, less on a slow server. An after-hook on
 * AFGCentralStorageContainer::Factory_Tick uploads the items the game's own timer says are still owed this tick,
 * through the game's own AFGCentralStorageSubsystem::UploadItemFromInventoryToCentralStorage (so the depot's limits
 * and item rules still apply). It runs only on the server or in single player, and only does anything when an item
 * takes less than one tick to upload. On by default; the console variable EasyDD.UploadTopUp turns it off.
 */
namespace EasyDD::UploadBoost
{
	/** Installs the hook (not in the editor). Called once from module startup. */
	void Start();
	void Stop();

	/** With "Allow extreme multipliers" on, an uploader's extra uploads per tick are not capped. */
	void SetExtremeMode(bool bExtreme);

	/** Appends status lines for EasyDD.Status. */
	void DescribeStatus(TArray<FString>& OutLines);
}
