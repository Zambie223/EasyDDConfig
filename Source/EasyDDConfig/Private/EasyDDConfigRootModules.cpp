#include "EasyDDConfigRootModules.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/ModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "EasyDDAdminSync.h"
#include "EasyDDConfigConstants.h"
#include "EasyDDConfigRCO.h"
#include "EasyDDConfigLog.h"
#include "EasyDDConfigMath.h"
#include "EasyDDConfigSubsystem.h"
#include "Engine/GameInstance.h"

UEasyDDConfigGameInstanceModule::UEasyDDConfigGameInstanceModule()
{
	bRootModule = true;
	// The per-player channel a dedicated server's admin uses to send menu changes (SML registers it on the game mode).
	RemoteCallObjects.Add(UEasyDDConfigRCO::StaticClass());
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

	// The configuration is registered during INITIALIZATION, so its live values exist from here on.
	if (Phase == ELifecyclePhase::POST_INITIALIZATION)
	{
		// If the game ended while this player's menu showed a server's values, put their own values back first.
		const UGameInstance* GameInstance = GetGameInstance();
		UConfigManager* ConfigManager = GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
		FConfigId ConfigId;
		ConfigId.ModReference = EasyDD::ModReference;
		if (ConfigManager && EasyDD::AdminSync::HasBackup())
		{
			EasyDD::AdminSync::RestoreBackup(ConfigManager, ConfigManager->GetConfigurationRootSection(ConfigId),
				TEXT("the game ended last time while showing a server's values"));
		}
		UpdateStackTooltip();
	}
}

void UEasyDDConfigGameInstanceModule::UpdateStackTooltip()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UConfigManager* ConfigManager = GameInstance ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	FConfigId ConfigId;
	ConfigId.ModReference = EasyDD::ModReference;
	UConfigPropertySection* Root = ConfigManager ? ConfigManager->GetConfigurationRootSection(ConfigId) : nullptr;
	if (!Root)
	{
		return;
	}
	if (BoundRootSection.Get() != Root)
	{
		if (UConfigPropertySection* Old = BoundRootSection.Get())
		{
			Old->OnPropertyValueChanged.RemoveDynamic(this, &UEasyDDConfigGameInstanceModule::UpdateStackTooltip);
		}
		Root->OnPropertyValueChanged.AddUniqueDynamic(this, &UEasyDDConfigGameInstanceModule::UpdateStackTooltip);
		BoundRootSection = Root;
		bHaveBaseStackTooltip = false; // a re-registered configuration has fresh property objects
	}

	const TObjectPtr<UConfigProperty>* StackEntry = Root->SectionProperties.Find(EasyDD::KeyStackLimitMultiplier);
	UConfigPropertyFloat* Stack = StackEntry ? Cast<UConfigPropertyFloat>(StackEntry->Get()) : nullptr;
	const TObjectPtr<UConfigProperty>* ExtremeEntry = Root->SectionProperties.Find(EasyDD::KeyAllowExtremeMultipliers);
	const UConfigPropertyBool* Extreme = ExtremeEntry ? Cast<UConfigPropertyBool>(ExtremeEntry->Get()) : nullptr;
	if (!Stack)
	{
		return;
	}
	if (!bHaveBaseStackTooltip)
	{
		BaseStackTooltip = Stack->Tooltip;
		bHaveBaseStackTooltip = true;
	}
	if (!Extreme || !Extreme->Value)
	{
		Stack->Tooltip = BaseStackTooltip;
		return;
	}
	const int32 LargestStack = AEasyDDConfigSubsystem::GetLargestUploadableStackSize();
	const int32 MaxStacks = EasyDD::Math::MaxSafeTotalStacks(LargestStack);
	Stack->Tooltip = FText::FromString(FString::Printf(
		TEXT("%s\n\nAllow extreme multipliers is ON: up to x%s here. Technical limit: %s stacks of each item in total ")
		TEXT("(research included). The game can count at most 2,147,483,647 of one item, and the largest stack size of a ")
		TEXT("loaded item is %s. Larger totals are cut to the limit. In the main menu not every item is loaded yet, so the ")
		TEXT("limit can be lower in game."),
		*BaseStackTooltip.ToString(),
		*FText::AsNumber(static_cast<int32>(EasyDD::Math::MaxStackMultiplierExtreme)).ToString(),
		*FText::AsNumber(MaxStacks).ToString(),
		*FText::AsNumber(LargestStack).ToString()));
}

UEasyDDConfigGameWorldModule::UEasyDDConfigGameWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(AEasyDDConfigSubsystem::StaticClass());
}
