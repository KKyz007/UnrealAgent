// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAWorldCommands.h"
#include "UnrealAgent.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

TArray<FString> UAWorldCommands::GetSupportedMethods() const
{
	return {
		TEXT("get_world_outliner"),
		TEXT("get_current_level"),
		TEXT("get_actor_details"),
	};
}

TSharedPtr<FJsonObject> UAWorldCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("get_world_outliner"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> ClassProp = MakeShared<FJsonObject>();
		ClassProp->SetStringField(TEXT("type"), TEXT("string"));
		ClassProp->SetStringField(TEXT("description"), TEXT("Filter actors by class name"));
		Properties->SetObjectField(TEXT("class_filter"), ClassProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);
		return MakeToolSchema(TEXT("get_world_outliner"), TEXT("Get all actors in the current level with their basic properties"), InputSchema);
	}
	else if (MethodName == TEXT("get_current_level"))
	{
		return MakeToolSchema(TEXT("get_current_level"), TEXT("Get current level information including name, path, and streaming levels"));
	}
	else if (MethodName == TEXT("get_actor_details"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> NameProp = MakeShared<FJsonObject>();
		NameProp->SetStringField(TEXT("type"), TEXT("string"));
		NameProp->SetStringField(TEXT("description"), TEXT("Actor label name to look up"));
		Properties->SetObjectField(TEXT("actor_name"), NameProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("actor_name")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("get_actor_details"), TEXT("Get detailed properties of a specific actor by name"), InputSchema);
	}

	return nullptr;
}

bool UAWorldCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("get_world_outliner")) return ExecuteGetWorldOutliner(Params, OutResult, OutError);
	if (MethodName == TEXT("get_current_level")) return ExecuteGetCurrentLevel(OutResult, OutError);
	if (MethodName == TEXT("get_actor_details")) return ExecuteGetActorDetails(Params, OutResult, OutError);

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAWorldCommands::ExecuteGetWorldOutliner(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return false;
	}

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	TArray<TSharedPtr<FJsonValue>> ActorList;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Apply class filter
		if (!ClassFilter.IsEmpty())
		{
			FString ClassName = Actor->GetClass()->GetName();
			if (!ClassName.Contains(ClassFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
		ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
		ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
		ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());

		// Location
		FVector Loc = Actor->GetActorLocation();
		TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
		LocObj->SetNumberField(TEXT("x"), Loc.X);
		LocObj->SetNumberField(TEXT("y"), Loc.Y);
		LocObj->SetNumberField(TEXT("z"), Loc.Z);
		ActorObj->SetObjectField(TEXT("location"), LocObj);

		// Rotation
		FRotator Rot = Actor->GetActorRotation();
		TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
		RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
		RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
		RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
		ActorObj->SetObjectField(TEXT("rotation"), RotObj);

		// Scale
		FVector Scale = Actor->GetActorScale3D();
		TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
		ScaleObj->SetNumberField(TEXT("x"), Scale.X);
		ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
		ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
		ActorObj->SetObjectField(TEXT("scale"), ScaleObj);

		ActorObj->SetBoolField(TEXT("is_hidden"), Actor->IsHidden());

		ActorList.Add(MakeShared<FJsonValueObject>(ActorObj));
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetArrayField(TEXT("actors"), ActorList);
	OutResult->SetNumberField(TEXT("count"), ActorList.Num());
	OutResult->SetStringField(TEXT("level"), World->GetMapName());

	return true;
}

bool UAWorldCommands::ExecuteGetCurrentLevel(TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return false;
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("name"), World->GetMapName());
	OutResult->SetStringField(TEXT("path"), World->GetPathName());

	// Streaming levels
	TArray<TSharedPtr<FJsonValue>> StreamingLevels;
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel)
		{
			TSharedPtr<FJsonObject> LevelObj = MakeShared<FJsonObject>();
			LevelObj->SetStringField(TEXT("name"), StreamingLevel->GetWorldAssetPackageFName().ToString());
			LevelObj->SetBoolField(TEXT("is_loaded"), StreamingLevel->IsLevelLoaded());
			LevelObj->SetBoolField(TEXT("is_visible"), StreamingLevel->IsLevelVisible());
			StreamingLevels.Add(MakeShared<FJsonValueObject>(LevelObj));
		}
	}
	OutResult->SetArrayField(TEXT("streaming_levels"), StreamingLevels);

	return true;
}

bool UAWorldCommands::ExecuteGetActorDetails(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		OutError = TEXT("Invalid params: 'actor_name' is required");
		return false;
	}

	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("No editor world available");
		return false;
	}

	// Find actor by label
	AActor* FoundActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName || (*It)->GetName() == ActorName)
		{
			FoundActor = *It;
			break;
		}
	}

	if (!FoundActor)
	{
		OutError = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
		return false;
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("name"), FoundActor->GetActorLabel());
	OutResult->SetStringField(TEXT("internal_name"), FoundActor->GetName());
	OutResult->SetStringField(TEXT("class"), FoundActor->GetClass()->GetName());
	OutResult->SetStringField(TEXT("path"), FoundActor->GetPathName());

	// Transform
	FVector Loc = FoundActor->GetActorLocation();
	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Loc.X);
	LocObj->SetNumberField(TEXT("y"), Loc.Y);
	LocObj->SetNumberField(TEXT("z"), Loc.Z);
	OutResult->SetObjectField(TEXT("location"), LocObj);

	FRotator Rot = FoundActor->GetActorRotation();
	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
	RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
	RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
	OutResult->SetObjectField(TEXT("rotation"), RotObj);

	FVector Scale = FoundActor->GetActorScale3D();
	TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
	ScaleObj->SetNumberField(TEXT("x"), Scale.X);
	ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
	ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
	OutResult->SetObjectField(TEXT("scale"), ScaleObj);

	// Components
	TArray<TSharedPtr<FJsonValue>> ComponentList;
	TArray<UActorComponent*> Components;
	FoundActor->GetComponents(Components);
	for (UActorComponent* Comp : Components)
	{
		if (Comp)
		{
			TSharedPtr<FJsonObject> CompObj = MakeShared<FJsonObject>();
			CompObj->SetStringField(TEXT("name"), Comp->GetName());
			CompObj->SetStringField(TEXT("class"), Comp->GetClass()->GetName());
			ComponentList.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}
	OutResult->SetArrayField(TEXT("components"), ComponentList);

	// Flags
	OutResult->SetBoolField(TEXT("is_hidden"), FoundActor->IsHidden());
	OutResult->SetBoolField(TEXT("is_editable"), !FoundActor->IsLockLocation());

	// Tags
	TArray<TSharedPtr<FJsonValue>> Tags;
	for (const FName& Tag : FoundActor->Tags)
	{
		Tags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	OutResult->SetArrayField(TEXT("tags"), Tags);

	return true;
}
