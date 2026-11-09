#include "EasyDDConfigRootModules.h"

#include "Configuration/ModConfiguration.h"
#include "EasyDDConfigConstants.h"
#include "EasyDDConfigLog.h"
#include "EasyDDConfigSubsystem.h"

UEasyDDConfigGameInstanceModule::UEasyDDConfigGameInstanceModule()
{
	bRootModule = true;
}

void UEasyDDConfigGameInstanceModule::DispatchLifecycleEvent(const ELifecyclePhase Phase)
{
	// Configurations are registered during INITIALIZATION, so add ours during CONSTRUCTION.
	// Loaded here instead of in the constructor so the mod's content is guaranteed to be mounted.
	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		const TSoftClassPtr<UModConfiguration> ConfigClass{FSoftObjectPath(EasyDD::ConfigClassPath)};
		if (UClass* Loaded = ConfigClass.LoadSynchronous())
		{
			ModConfigurations.AddUnique(Loaded);
		}
		else
		{
			UE_LOG(LogEasyDDConfig, Error, TEXT("Could not load the Mod Configuration %s; settings will stay at vanilla (1.0)"), EasyDD::ConfigClassPath);
		}
	}
	Super::DispatchLifecycleEvent(Phase);
}

UEasyDDConfigGameWorldModule::UEasyDDConfigGameWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(AEasyDDConfigSubsystem::StaticClass());
}
