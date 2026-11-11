#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "EasyDDConfigRCO.generated.h"

/**
 * Per-player channel for Easy DD Config (SML registers one per player controller).
 * A dedicated server's admin sends a setting they changed in their Mods menu; the server checks the sender's privilege
 * itself and answers with the result, which the admin sees in chat.
 */
UCLASS(NotBlueprintable)
class EASYDDCONFIG_API UEasyDDConfigRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Asks the server to set one setting. Only dedicated servers accept it, and only from an Administrator. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetConfigValue(const FString& Key, const FString& Value);

	/** The server's answer to Server_SetConfigValue. */
	UFUNCTION(Client, Reliable)
	void Client_ConfigResult(bool bAccepted, const FString& Message);

private:
	/** Remote call objects need a replicated property (the game's own RCOs carry one too). */
	UPROPERTY(Replicated)
	bool mForceNetField_UEasyDDConfigRCO = false;
};
