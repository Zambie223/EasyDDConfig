#pragma once

#include "CoreMinimal.h"

/**
 * Makes the game's displayed depot upload rate exact (e.g. 4,800 per min instead of 4,799.999 at x20 with all research).
 *
 * The game computes the effective upload time as base - base * (research% / 100) in float, which near the exact value
 * comes in steps too coarse to ever give the exact rate, so no base value written by the mod can fix it. Instead, an
 * after-call hook on the Blueprint-readable AFGCentralStorageSubsystem::GetCentralStorageTimeToUpload snaps a time
 * whose rate is within float error of a multiple of 0.5 items/min to the time of that rate. Every rate the settings
 * produce is such a multiple (15/min x a multiplier in tenths x 2 per research tier = multiples of 1.5), and the change
 * is at most a few millionths of the value, so uploads are unaffected. Anything else passes through unchanged.
 * Simulated against the game's formula: every normal setting (x0.1 to x20, all research levels) displays exactly;
 * extreme settings above ~8,000/min can show a small fraction (float limit). Not effective if the game inlined the
 * getter into its UI path; EasyDD.Status calls it and shows the result.
 */
namespace EasyDD::DisplayRounding
{
	/** Installs the hook (not in the editor). Called once from module startup. */
	void Start();
	void Stop();

	/** The snapping itself, exposed for EasyDD.Status. */
	float SnapTimeToUpload(float TimeToUpload);
}
