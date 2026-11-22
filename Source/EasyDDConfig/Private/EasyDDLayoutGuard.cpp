#include "EasyDDLayoutGuard.h"

#include "EasyDDConfigLog.h"
#include "FGChatManager.h"
#include "FGRemoteCallObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace EasyDD::Layout
{
	bool Matches(const UStruct* Type, const int32 CompiledSize, const std::initializer_list<FField> Fields, const TCHAR* Feature)
	{
		if (!Type)
		{
			UE_LOG(LogEasyDDConfig, Warning, TEXT("%s is off: a game type it needs was not found (the game may have changed). Look for an Easy DD Config update."), Feature);
			return false;
		}
		// The size the running game allocates for this type (its own sizeof).
		const UScriptStruct* Struct = Cast<UScriptStruct>(Type);
		const int32 RuntimeSize = Struct ? Struct->GetStructureSize() : Type->GetPropertiesSize();
		if (RuntimeSize != CompiledSize)
		{
			UE_LOG(LogEasyDDConfig, Warning, TEXT("%s is off: %s is %d bytes in this game version but %d in the version Easy DD Config was built for. Look for an Easy DD Config update."),
				Feature, *Type->GetName(), RuntimeSize, CompiledSize);
			return false;
		}
		for (const FField& Field : Fields)
		{
			const FProperty* Property = FindFProperty<FProperty>(Type, Field.Name);
			const int32 RuntimeOffset = Property ? Property->GetOffset_ForInternal() : -1;
			if (RuntimeOffset != Field.CompiledOffset)
			{
				UE_LOG(LogEasyDDConfig, Warning, TEXT("%s is off: %s::%s is at offset %d in this game version but %d in the version Easy DD Config was built for. Look for an Easy DD Config update."),
					Feature, *Type->GetName(), Field.Name, RuntimeOffset, Field.CompiledOffset);
				return false;
			}
		}
		return true;
	}

	bool ChatMessageMatches()
	{
		static const bool bMatches = Matches(FChatMessageStruct::StaticStruct(), sizeof(FChatMessageStruct),
			{ EASYDD_FIELD(FChatMessageStruct, MessageText), EASYDD_FIELD(FChatMessageStruct, MessageSender), EASYDD_FIELD(FChatMessageStruct, MessageType) },
			TEXT("Easy DD Config chat messages"));
		return bMatches;
	}

	bool RemoteCallObjectMatches()
	{
		static const bool bMatches = Matches(UFGRemoteCallObject::StaticClass(), sizeof(UFGRemoteCallObject), {}, TEXT("Admin menu sync"));
		return bMatches;
	}

	bool EngineBaseMatches()
	{
		static const bool bMatches = Matches(AActor::StaticClass(), sizeof(AActor), {}, TEXT("Easy DD Config"))
			&& Matches(UWorld::StaticClass(), sizeof(UWorld), {}, TEXT("Easy DD Config"));
		return bMatches;
	}
}
