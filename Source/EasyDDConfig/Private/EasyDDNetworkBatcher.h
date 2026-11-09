#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Merges the Dimensional Depot's item-count messages on the host or dedicated server.
 *
 * The base game sends one reliable multicast per depot amount change, and a player whose game falls behind is
 * disconnected once 511 of them are unacknowledged ("Outgoing reliable buffer overflow"). Faster uploads make that
 * more likely. This keeps the latest amount per item and sends the changed ones about once a second, holding back
 * while a player's queue is half full. The depot contents themselves replicate as a property; these messages only
 * refresh UI. Follows the design of the Depot Network Fix mod, used with its author's permission.
 */
namespace EasyDD::NetBatch
{
	/** Installs the hook and the flush ticker. Does nothing in the editor or when Depot Network Fix is installed. */
	void Start();

	/** Removes the hook and the ticker and drops unsent updates. */
	void Stop();

	/** Turns batching of new messages on or off (the BatchDepotNetworkUpdates setting). Pending ones are still sent. */
	void SetEnabled(bool bEnabled);
	bool IsEnabled();

	/** Appends human-readable status lines for EasyDD.Status. */
	void DescribeStatus(const UWorld* World, TArray<FString>& OutLines);
}
