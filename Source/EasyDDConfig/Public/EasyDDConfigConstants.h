#pragma once

#include "CoreMinimal.h"

namespace EasyDD
{
	/** Must match the plugin name; SML uses it as the config file name (Configs/EasyDDConfig.cfg). */
	inline constexpr const TCHAR* ModReference = TEXT("EasyDDConfig");

	/** Mod Configuration asset, created by Tools/create_config_asset.py. */
	inline constexpr const TCHAR* ConfigClassPath = TEXT("/EasyDDConfig/EasyDDConfiguration.EasyDDConfiguration_C");

	/** Keys in the configuration's root section. Changing these renames the values in users' config files. */
	inline constexpr const TCHAR* KeyUploadSpeedMultiplier = TEXT("UploadSpeedMultiplier");
	inline constexpr const TCHAR* KeyStackLimitMultiplier = TEXT("StackLimitMultiplier");
	inline constexpr const TCHAR* KeyBatchDepotNetworkUpdates = TEXT("BatchDepotNetworkUpdates");
}
