#pragma once

#include "CoreMinimal.h"


/**
 * Pure helpers for turning the user's multipliers into the values written to UFGCentralStorageSettings.
 * No game state is touched here.
 *
 * The game's formulas are inferred from FactoryGame header comments, because the implementation is not public:
 *   upload time  = mDefaultTimeToUpload * (1 - min(researchPercent, mMaxTimeToUploadPercentageDecrease) / 100)
 *   stack limit  = mDefaultItemStackLimit + researchBonusStacks
 */
namespace EasyDD::Math
{
	constexpr float DefaultMultiplier = 1.0f;

	constexpr float MinUploadMultiplier = 0.1f;
	constexpr float MaxUploadMultiplier = 20.0f;

	/** Below 1 the compensated base could go negative, so stacks can only be raised. */
	constexpr float MinStackMultiplier = 1.0f;
	constexpr float MaxStackMultiplier = 100.0f;

	/** Hard cap on total stacks per item type. Keeps stacks * stack size far from int32 overflow, even with stack-size mods. */
	constexpr int32 MaxTotalStacks = 500;

	/**
	 * With "Allow extreme multipliers" on, the menu limits above are replaced by these, which only keep the numbers
	 * meaningful. The real limits then come from the game: the per-tick upload top-up cap (see EasyDDUploadBooster) and
	 * the overflow-safe stack total (MaxSafeTotalStacks).
	 */
	constexpr float MaxUploadMultiplierExtreme = 10000.0f;
	constexpr float MaxStackMultiplierExtreme = 100000.0f;

	/** Bounds for the multipliers, depending on the "Allow extreme multipliers" switch. */
	struct FMultiplierBounds
	{
		float MinUpload = MinUploadMultiplier;
		float MaxUpload = MaxUploadMultiplier;
		float MinStack = MinStackMultiplier;
		float MaxStack = MaxStackMultiplier;

		static FMultiplierBounds For(const bool bExtreme)
		{
			FMultiplierBounds Bounds;
			if (bExtreme)
			{
				Bounds.MaxUpload = MaxUploadMultiplierExtreme;
				Bounds.MaxStack = MaxStackMultiplierExtreme;
			}
			return Bounds;
		}
	};

	/**
	 * Largest total stack count per item type for which the game's per-item limit (stacks * stack size, an int32) can't
	 * overflow, for the largest stack size of any uploadable item. No safety margin: this applies only with "Allow
	 * extreme multipliers" on, whose users accept the risk, so the full int32 range is allowed.
	 */
	inline int32 MaxSafeTotalStacks(const int32 LargestStackSize)
	{
		const int64 StackSize = FMath::Max<int64>(LargestStackSize, 1);
		return static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(MAX_int32) / StackSize, 1, MAX_int32));
	}

	/** The game's own values, captured before this mod writes anything. */
	struct FVanillaValues
	{
		float TimeToUpload = 0.0f;
		float MaxTimeToUploadPercentageDecrease = 0.0f;
		int32 ItemStackLimit = 0;

		bool IsValid() const
		{
			return FMath::IsFinite(TimeToUpload) && TimeToUpload > 0.0f
				&& FMath::IsFinite(MaxTimeToUploadPercentageDecrease)
				&& MaxTimeToUploadPercentageDecrease >= 0.0f && MaxTimeToUploadPercentageDecrease < 100.0f
				&& ItemStackLimit > 0;
		}
	};

	/** Rounds to one decimal place, the precision the mod applies (dragging a Mod Options box gives values like 71.618). */
	inline float RoundToTenth(const float Value)
	{
		return FMath::RoundToFloat(Value * 10.0f) / 10.0f;
	}

	/** True when the raw value will be clamped (or replaced, if not finite) rather than just rounded. */
	inline bool IsOutsideRange(const float Value, const float Min, const float Max)
	{
		return !FMath::IsFinite(Value) || Value < Min || Value > Max;
	}

	/**
	 * Non-finite values fall back to 1.0 (vanilla); everything else is rounded to one decimal place, then clamped to
	 * [Min, Max]. The bounds are whole tenths, so the result is always a tenth.
	 */
	inline float SanitizeMultiplier(const float Value, const float Min, const float Max)
	{
		if (!FMath::IsFinite(Value))
		{
			return DefaultMultiplier;
		}
		return FMath::Clamp(RoundToTenth(Value), Min, Max);
	}

	/**
	 * Base upload time for a given multiplier. Research then removes its percentage as usual.
	 *
	 * Known cosmetic limit: the game computes the effective time as base - base * (research% / 100) in float (identified
	 * in 0.3.0 testing: x20 with all research gives 0.012500003 s), so near 0.0125 its results come in steps of about
	 * 1.5e-8 s and none displays as exactly 4,800 per min; the game shows "4,799.999". No base value avoids that, so the
	 * plain quotient is used.
	 */
	inline float ComputeDefaultTimeToUpload(const float VanillaTimeToUpload, const float SanitizedUploadMultiplier)
	{
		return VanillaTimeToUpload / SanitizedUploadMultiplier;
	}

	/**
	 * Base stack limit that makes (base + research bonus) equal round(multiplier * (vanilla base + research bonus)).
	 * The total never drops below vanilla and never exceeds TotalCap (unless vanilla already does).
	 * Returned value is always >= VanillaBase, so it is always >= 1.
	 */
	inline int32 ComputeDefaultItemStackLimit(const int32 VanillaBase, const int32 ResearchBonus, const float SanitizedStackMultiplier, const int32 TotalCap = MaxTotalStacks)
	{
		const int32 Bonus = FMath::Max(ResearchBonus, 0);
		const int32 VanillaTotal = VanillaBase + Bonus;
		if (VanillaTotal >= TotalCap)
		{
			return VanillaBase;
		}
		const double Scaled = FMath::Min(static_cast<double>(SanitizedStackMultiplier) * VanillaTotal, static_cast<double>(TotalCap));
		const int32 Total = FMath::Clamp(static_cast<int32>(FMath::RoundToDouble(Scaled)), VanillaTotal, TotalCap);
		return Total - Bonus;
	}

	/** Items per minute for a given per-item upload time. */
	inline float ItemsPerMinute(const float TimeToUpload)
	{
		return TimeToUpload > 0.0f ? 60.0f / TimeToUpload : 0.0f;
	}
}
