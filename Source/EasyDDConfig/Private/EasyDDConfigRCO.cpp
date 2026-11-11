#include "EasyDDConfigRCO.h"

#include "EasyDDAdminSync.h"
#include "EasyDDConfigLog.h"
#include "EasyDDConfigSubsystem.h"
#include "Engine/World.h"
#include "FGChatManager.h"
#include "FGPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Server/FGDSAuthenticationTypes.h"

void UEasyDDConfigRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEasyDDConfigRCO, mForceNetField_UEasyDDConfigRCO);
}

bool UEasyDDConfigRCO::Server_SetConfigValue_Validate(const FString& Key, const FString& Value)
{
	// Malformed requests drop the connection; legitimate ones are short. Refusals of well-formed requests happen below.
	return Key.Len() <= 64 && Value.Len() <= 64;
}

void UEasyDDConfigRCO::Server_SetConfigValue_Implementation(const FString& Key, const FString& Value)
{
	UWorld* World = GetWorld();
	AFGPlayerController* Sender = GetOwnerPlayerController();
	const FString Who = (Sender && Sender->PlayerState) ? Sender->PlayerState->GetPlayerName() : FString(TEXT("unknown player"));

	FString Message;
	bool bAccepted = false;
	if (!World || World->GetNetMode() != NM_DedicatedServer)
	{
		Message = TEXT("Refused: only dedicated servers take settings from an admin's menu. A host uses their own Mods menu.");
	}
	else if (!Sender || Sender->GetDSPrivilegeLevel() != EPrivilegeLevel::Administrator)
	{
		Message = TEXT("Refused: only server admins can change the server's Easy DD Config settings.");
	}
	else if (!AEasyDDConfigSubsystem::ReadAllowAdminMenuChanges(World))
	{
		Message = TEXT("Refused: this server's owner has turned off changes from the admin menu (AllowAdminMenuChanges = false).");
	}
	else if (!EasyDD::AdminSync::IsSyncedKey(Key))
	{
		Message = FString::Printf(TEXT("Refused: '%s' can't be changed from the menu."), *Key);
	}
	else
	{
		bAccepted = AEasyDDConfigSubsystem::SetConfigValue(World, Key, Value, Message);
	}
	UE_LOG(LogEasyDDConfig, Log, TEXT("Admin menu request from %s: %s = %s -> %s"), *Who, *Key, *Value, *Message);
	Client_ConfigResult(bAccepted, Message);
}

void UEasyDDConfigRCO::Client_ConfigResult_Implementation(const bool bAccepted, const FString& Message)
{
	UE_LOG(LogEasyDDConfig, Log, TEXT("Server answered: %s"), *Message);
	if (!bAccepted)
	{
		if (AEasyDDConfigSubsystem* Subsystem = AEasyDDConfigSubsystem::Get(GetWorld()))
		{
			Subsystem->OnAdminRequestRefused(); // put the server's values back into the menu
		}
	}
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct Line;
		Line.MessageType = EFGChatMessageType::CMT_SystemMessage;
		Line.MessageSender = FText::FromString(TEXT("Easy DD Config"));
		Line.MessageText = FText::FromString(bAccepted ? FString::Printf(TEXT("Server: %s"), *Message) : Message);
		Chat->AddChatMessageToReceived(Line);
	}
}
