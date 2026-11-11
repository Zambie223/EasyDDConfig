#include "EasyDDAdminSync.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Dom/JsonObject.h"
#include "EasyDDConfigConstants.h"
#include "EasyDDConfigLog.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	template <typename T>
	T* FindTyped(UConfigPropertySection* Root, const TCHAR* Key)
	{
		if (!Root)
		{
			return nullptr;
		}
		const TObjectPtr<UConfigProperty>* Entry = Root->SectionProperties.Find(Key);
		return Entry ? Cast<T>(Entry->Get()) : nullptr;
	}

	bool NearlyEqual(const float A, const float B)
	{
		return FMath::IsNearlyEqual(A, B, 1.0e-4f);
	}
}

namespace EasyDD::AdminSync
{
	bool IsSyncedKey(const FString& Key)
	{
		for (const TCHAR* Synced : {KeyUploadSpeedMultiplier, KeyStackLimitMultiplier, KeyBatchDepotNetworkUpdates, KeyAllowExtremeMultipliers})
		{
			if (Key.Equals(Synced, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	bool Read(UConfigPropertySection* Root, FValues& Out)
	{
		const UConfigPropertyFloat* Upload = FindTyped<UConfigPropertyFloat>(Root, KeyUploadSpeedMultiplier);
		const UConfigPropertyFloat* Stack = FindTyped<UConfigPropertyFloat>(Root, KeyStackLimitMultiplier);
		const UConfigPropertyBool* Batching = FindTyped<UConfigPropertyBool>(Root, KeyBatchDepotNetworkUpdates);
		const UConfigPropertyBool* Extreme = FindTyped<UConfigPropertyBool>(Root, KeyAllowExtremeMultipliers);
		if (!Upload || !Stack || !Batching || !Extreme)
		{
			return false;
		}
		Out.Upload = Upload->Value;
		Out.Stack = Stack->Value;
		Out.bBatching = Batching->Value;
		Out.bExtreme = Extreme->Value;
		return true;
	}

	bool Write(UConfigPropertySection* Root, const FValues& In)
	{
		bool bChanged = false;
		if (UConfigPropertyFloat* Upload = FindTyped<UConfigPropertyFloat>(Root, KeyUploadSpeedMultiplier); Upload && !NearlyEqual(Upload->Value, In.Upload))
		{
			Upload->Value = In.Upload;
			bChanged = true;
		}
		if (UConfigPropertyFloat* Stack = FindTyped<UConfigPropertyFloat>(Root, KeyStackLimitMultiplier); Stack && !NearlyEqual(Stack->Value, In.Stack))
		{
			Stack->Value = In.Stack;
			bChanged = true;
		}
		if (UConfigPropertyBool* Batching = FindTyped<UConfigPropertyBool>(Root, KeyBatchDepotNetworkUpdates); Batching && Batching->Value != In.bBatching)
		{
			Batching->Value = In.bBatching;
			bChanged = true;
		}
		if (UConfigPropertyBool* Extreme = FindTyped<UConfigPropertyBool>(Root, KeyAllowExtremeMultipliers); Extreme && Extreme->Value != In.bExtreme)
		{
			Extreme->Value = In.bExtreme;
			bChanged = true;
		}
		return bChanged;
	}

	TArray<TPair<FString, FString>> Diff(const FValues& From, const FValues& To)
	{
		TArray<TPair<FString, FString>> Out;
		if (!NearlyEqual(From.Upload, To.Upload))
		{
			Out.Emplace(KeyUploadSpeedMultiplier, FString::SanitizeFloat(To.Upload));
		}
		if (!NearlyEqual(From.Stack, To.Stack))
		{
			Out.Emplace(KeyStackLimitMultiplier, FString::SanitizeFloat(To.Stack));
		}
		if (From.bBatching != To.bBatching)
		{
			Out.Emplace(KeyBatchDepotNetworkUpdates, To.bBatching ? TEXT("true") : TEXT("false"));
		}
		if (From.bExtreme != To.bExtreme)
		{
			Out.Emplace(KeyAllowExtremeMultipliers, To.bExtreme ? TEXT("true") : TEXT("false"));
		}
		return Out;
	}

	FString BackupFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EasyDDConfig"), TEXT("OwnSettingsWhileServerAdmin.json"));
	}

	bool HasBackup()
	{
		return FPaths::FileExists(BackupFilePath());
	}

	bool SaveBackup(const FValues& Own)
	{
		const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(KeyUploadSpeedMultiplier, Own.Upload);
		Json->SetNumberField(KeyStackLimitMultiplier, Own.Stack);
		Json->SetBoolField(KeyBatchDepotNetworkUpdates, Own.bBatching);
		Json->SetBoolField(KeyAllowExtremeMultipliers, Own.bExtreme);
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		if (!FJsonSerializer::Serialize(Json, Writer) || !FFileHelper::SaveStringToFile(Text, *BackupFilePath()))
		{
			UE_LOG(LogEasyDDConfig, Error, TEXT("Could not save your own settings to %s; not showing the server's values in your menu"), *BackupFilePath());
			return false;
		}
		return true;
	}

	bool RestoreBackup(UConfigManager* ConfigManager, UConfigPropertySection* Root, const TCHAR* Why)
	{
		const FString Path = BackupFilePath();
		FString Text;
		if (!Root || !FPaths::FileExists(Path) || !FFileHelper::LoadFileToString(Text, *Path))
		{
			return false;
		}
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FValues Own;
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid() || !Read(Root, Own))
		{
			UE_LOG(LogEasyDDConfig, Error, TEXT("Could not read %s; your own Easy DD Config settings were not restored. The file is kept."), *Path);
			return false;
		}
		double Number = 0.0;
		if (Json->TryGetNumberField(KeyUploadSpeedMultiplier, Number)) { Own.Upload = static_cast<float>(Number); }
		if (Json->TryGetNumberField(KeyStackLimitMultiplier, Number)) { Own.Stack = static_cast<float>(Number); }
		Json->TryGetBoolField(KeyBatchDepotNetworkUpdates, Own.bBatching);
		Json->TryGetBoolField(KeyAllowExtremeMultipliers, Own.bExtreme);

		Write(Root, Own);
		Root->MarkDirty();
		if (ConfigManager)
		{
			ConfigManager->FlushPendingSaves(); // on disk before the backup goes
		}
		IFileManager::Get().Delete(*Path);
		UE_LOG(LogEasyDDConfig, Log, TEXT("Restored your own Easy DD Config settings (%s): upload x%g, stacks x%g, batching %s, extreme %s"),
			Why, Own.Upload, Own.Stack, Own.bBatching ? TEXT("on") : TEXT("off"), Own.bExtreme ? TEXT("on") : TEXT("off"));
		return true;
	}
}
