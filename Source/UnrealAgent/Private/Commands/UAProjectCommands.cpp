// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAProjectCommands.h"
#include "UnrealAgent.h"

#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Editor.h"
#include "Selection.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

TArray<FString> UAProjectCommands::GetSupportedMethods() const
{
	return {
		TEXT("get_project_info"),
		TEXT("get_editor_state"),
	};
}

TSharedPtr<FJsonObject> UAProjectCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("get_project_info"))
	{
		return MakeToolSchema(
			TEXT("get_project_info"),
			TEXT("Get detailed information about the current Unreal project including name, engine version, modules, and plugins")
		);
	}
	else if (MethodName == TEXT("get_editor_state"))
	{
		return MakeToolSchema(
			TEXT("get_editor_state"),
			TEXT("Get current editor state including active level, PIE status, and selected actors")
		);
	}

	return nullptr;
}

bool UAProjectCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("get_project_info"))
	{
		return ExecuteGetProjectInfo(OutResult, OutError);
	}
	else if (MethodName == TEXT("get_editor_state"))
	{
		return ExecuteGetEditorState(OutResult, OutError);
	}

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAProjectCommands::ExecuteGetProjectInfo(TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	OutResult = MakeShared<FJsonObject>();

	// Project name
	OutResult->SetStringField(TEXT("project_name"), FApp::GetProjectName());

	// Engine version
	OutResult->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());

	// Project directory
	OutResult->SetStringField(TEXT("project_dir"), FPaths::ProjectDir());

	// Read .uproject file for modules and plugins
	FString UProjectPath = FPaths::GetProjectFilePath();
	FString UProjectContent;

	if (FFileHelper::LoadFileToString(UProjectContent, *UProjectPath))
	{
		TSharedPtr<FJsonObject> UProjectJson;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(UProjectContent);

		if (FJsonSerializer::Deserialize(Reader, UProjectJson) && UProjectJson.IsValid())
		{
			// Modules
			const TArray<TSharedPtr<FJsonValue>>* ModulesArray;
			if (UProjectJson->TryGetArrayField(TEXT("Modules"), ModulesArray))
			{
				TArray<TSharedPtr<FJsonValue>> ModuleNames;
				for (const auto& Module : *ModulesArray)
				{
					if (const TSharedPtr<FJsonObject>* ModuleObj; Module->TryGetObject(ModuleObj))
					{
						FString Name;
						if ((*ModuleObj)->TryGetStringField(TEXT("Name"), Name))
						{
							ModuleNames.Add(MakeShared<FJsonValueString>(Name));
						}
					}
				}
				OutResult->SetArrayField(TEXT("modules"), ModuleNames);
			}

			// Plugins
			const TArray<TSharedPtr<FJsonValue>>* PluginsArray;
			if (UProjectJson->TryGetArrayField(TEXT("Plugins"), PluginsArray))
			{
				TArray<TSharedPtr<FJsonValue>> PluginInfos;
				for (const auto& Plugin : *PluginsArray)
				{
					if (const TSharedPtr<FJsonObject>* PluginObj; Plugin->TryGetObject(PluginObj))
					{
						TSharedPtr<FJsonObject> PluginInfo = MakeShared<FJsonObject>();
						FString Name;
						bool bEnabled = false;

						if ((*PluginObj)->TryGetStringField(TEXT("Name"), Name))
						{
							PluginInfo->SetStringField(TEXT("name"), Name);
						}
						if ((*PluginObj)->TryGetBoolField(TEXT("Enabled"), bEnabled))
						{
							PluginInfo->SetBoolField(TEXT("enabled"), bEnabled);
						}

						PluginInfos.Add(MakeShared<FJsonValueObject>(PluginInfo));
					}
				}
				OutResult->SetArrayField(TEXT("plugins"), PluginInfos);
			}
		}
	}

	return true;
}

bool UAProjectCommands::ExecuteGetEditorState(TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	OutResult = MakeShared<FJsonObject>();

	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	// Current level/map name
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (EditorWorld)
	{
		OutResult->SetStringField(TEXT("current_level"), EditorWorld->GetMapName());
		OutResult->SetStringField(TEXT("world_path"), EditorWorld->GetPathName());
	}

	// PIE (Play In Editor) state
	OutResult->SetBoolField(TEXT("is_playing"), GEditor->IsPlayingSessionInEditor());

	// Selected actors
	TArray<TSharedPtr<FJsonValue>> SelectedActors;
	USelection* Selection = GEditor->GetSelectedActors();
	if (Selection)
	{
		for (int32 i = 0; i < Selection->Num(); ++i)
		{
			AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
			if (Actor)
			{
				TSharedPtr<FJsonObject> ActorInfo = MakeShared<FJsonObject>();
				ActorInfo->SetStringField(TEXT("name"), Actor->GetActorLabel());
				ActorInfo->SetStringField(TEXT("class"), Actor->GetClass()->GetName());

				FVector Location = Actor->GetActorLocation();
				TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
				LocationObj->SetNumberField(TEXT("x"), Location.X);
				LocationObj->SetNumberField(TEXT("y"), Location.Y);
				LocationObj->SetNumberField(TEXT("z"), Location.Z);
				ActorInfo->SetObjectField(TEXT("location"), LocationObj);

				SelectedActors.Add(MakeShared<FJsonValueObject>(ActorInfo));
			}
		}
	}
	OutResult->SetArrayField(TEXT("selected_actors"), SelectedActors);
	OutResult->SetNumberField(TEXT("selected_count"), SelectedActors.Num());

	return true;
}
