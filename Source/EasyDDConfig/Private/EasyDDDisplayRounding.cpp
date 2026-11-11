#include "EasyDDDisplayRounding.h"

#include "EasyDDConfigLog.h"
#include "FGCentralStorageSubsystem.h"
#include "Patching/NativeHookManager.h"

#include <cmath>

// Names in this anonymous namespace must be unique across the module: UBT's unity build merges .cpp files.
namespace
{
	using FDisplayTimeGetter = float (AFGCentralStorageSubsystem::*)() const;
	using FDisplayTimeGetterHook = HookInvoker<FDisplayTimeGetter, &AFGCentralStorageSubsystem::GetCentralStorageTimeToUpload>;
	constexpr const TCHAR* DisplayTimeGetterName = TEXT("AFGCentralStorageSubsystem::GetCentralStorageTimeToUpload");

	bool GDisplayHookInstalled = false;
	FDelegateHandle GDisplayHookHandle;

	void OnGetCentralStorageTimeToUpload(TCallScope<float(*)(const AFGCentralStorageSubsystem*)>& Scope, const AFGCentralStorageSubsystem* Self)
	{
		Scope.Override(EasyDD::DisplayRounding::SnapTimeToUpload(Scope(Self)));
	}
}

namespace EasyDD::DisplayRounding
{
	float SnapTimeToUpload(const float TimeToUpload)
	{
		if (!(TimeToUpload > 0.0f) || !FMath::IsFinite(TimeToUpload))
		{
			return TimeToUpload;
		}
		const double Rate = 60.0 / static_cast<double>(TimeToUpload);
		// Snap to multiples of 0.5/min; above 100,000/min (extreme multipliers) the step grows with the rate so the
		// game's float error (up to ~1.6/min at 2.4 million) can't land on the wrong multiple.
		const double Step = 0.5 * FMath::Pow(10.0, FMath::Max(0.0, FMath::CeilToDouble(FMath::LogX(10.0, Rate)) - 5.0));
		const double Nice = FMath::RoundToDouble(Rate / Step) * Step;
		// Float error in the game's formula is at most about 1.2e-6 of the value (one step of the base time over the
		// effective time at x20 with all research); 2e-6 covers it, and 0.001/min covers tiny rates.
		const double Tolerance = FMath::Max(0.001, Rate * 2.0e-6);
		if (Nice <= 0.0 || FMath::Abs(Rate - Nice) > Tolerance)
		{
			return TimeToUpload;
		}
		// Of the floats around the exact time, prefer the one whose rate is at most 0.0005/min ABOVE Nice: that displays
		// as Nice whether the game rounds or truncates to 3 decimals (the in-game test showed a rate a hair below, e.g.
		// 4,799.99993, is possible otherwise). If none qualifies, take the closest. Every normal setting (up to x20,
		// 4,800/min) then displays exactly; above ~8,000/min a float time can't hold the rate to 3 decimals, so extreme
		// settings can still show a tiny fraction.
		const float Centre = static_cast<float>(60.0 / Nice);
		float Closest = Centre;
		double ClosestError = FMath::Abs(60.0 / static_cast<double>(Centre) - Nice);
		float Above = 0.0f;
		double AboveBy = 0.0005;
		auto Consider = [&](const float Candidate)
		{
			if (!(Candidate > 0.0f))
			{
				return;
			}
			const double Diff = 60.0 / static_cast<double>(Candidate) - Nice;
			if (FMath::Abs(Diff) < ClosestError)
			{
				Closest = Candidate;
				ClosestError = FMath::Abs(Diff);
			}
			if (Diff >= 0.0 && Diff < AboveBy)
			{
				Above = Candidate;
				AboveBy = Diff;
			}
		};
		Consider(Centre);
		float Down = Centre;
		float Up = Centre;
		for (int32 Steps = 0; Steps < 8; ++Steps)
		{
			Down = std::nextafter(Down, 0.0f);
			Up = std::nextafter(Up, FLT_MAX);
			Consider(Down);
			Consider(Up);
		}
		return Above > 0.0f ? Above : Closest;
	}

	void Start()
	{
		if (GDisplayHookInstalled || WITH_EDITOR)
		{
			return;
		}
		FDisplayTimeGetterHook::InstallHook(DisplayTimeGetterName);
		GDisplayHookHandle = FDisplayTimeGetterHook::AddHandlerBefore(&OnGetCentralStorageTimeToUpload);
		GDisplayHookInstalled = true;
		UE_LOG(LogEasyDDConfig, Log, TEXT("Upload rate display: hooked %s"), DisplayTimeGetterName);
	}

	void Stop()
	{
		if (GDisplayHookHandle.IsValid())
		{
			FDisplayTimeGetterHook::RemoveHandler(DisplayTimeGetterName, GDisplayHookHandle);
			GDisplayHookHandle.Reset();
		}
		GDisplayHookInstalled = false;
	}
}
