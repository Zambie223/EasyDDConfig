#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "Module/GameWorldModule.h"
#include "EasyDDConfigRootModules.generated.h"

/**
 * Root game instance module (SML discovers native root modules as well as Blueprint ones).
 * Registers the Mod Configuration asset, which has to be a Blueprint because SML's config UI widgets come from
 * its Blueprint property classes.
 */
UCLASS()
class EASYDDCONFIG_API UEasyDDConfigGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	UEasyDDConfigGameInstanceModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};

/** Root game world module: spawns the depot subsystem in game worlds (never in the main menu). */
UCLASS()
class EASYDDCONFIG_API UEasyDDConfigGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	UEasyDDConfigGameWorldModule();
};
