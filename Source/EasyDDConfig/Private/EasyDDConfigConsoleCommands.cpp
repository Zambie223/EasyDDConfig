// Console commands. On a dedicated server they can be run from the Server Manager console, which the game
// restricts to administrators. Set and Reload are refused on clients.

#include "CoreMinimal.h"
#include "EasyDDConfigConstants.h"
#include "EasyDDConfigSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	AEasyDDConfigSubsystem* FindSubsystem(UWorld* World, FOutputDevice& Ar)
	{
		AEasyDDConfigSubsystem* Subsystem = AEasyDDConfigSubsystem::Get(World);
		if (!Subsystem)
		{
			Ar.Log(TEXT("Easy DD Config is not active in this world (main menu, or not replicated to this client yet)."));
		}
		return Subsystem;
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GStatusCommand(
		TEXT("EasyDD.Status"),
		TEXT("Shows Easy DD Config's vanilla, configured and effective Dimensional Depot values."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			if (const AEasyDDConfigSubsystem* Subsystem = FindSubsystem(World, Ar))
			{
				Subsystem->DescribeStatus(Ar);
			}
		}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GSetCommand(
		TEXT("EasyDD.Set"),
		TEXT("EasyDD.Set <UploadSpeedMultiplier|StackLimitMultiplier|BatchDepotNetworkUpdates> <Value> - changes and saves a setting on the host or server."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			if (Args.Num() != 2)
			{
				Ar.Logf(TEXT("Usage: EasyDD.Set <%s|%s> <number>, or EasyDD.Set %s <true|false>"),
					EasyDD::KeyUploadSpeedMultiplier, EasyDD::KeyStackLimitMultiplier, EasyDD::KeyBatchDepotNetworkUpdates);
				return;
			}
			FString Message;
			AEasyDDConfigSubsystem::SetConfigValue(World, Args[0], Args[1], Message);
			Ar.Log(Message);
		}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice GReloadCommand(
		TEXT("EasyDD.Reload"),
		TEXT("Re-reads Configs/EasyDDConfig.cfg (only this mod's file) and applies it on the host or server."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			FString Message;
			AEasyDDConfigSubsystem::ReloadConfigFromDisk(World, Message);
			Ar.Log(Message);
		}));
}
