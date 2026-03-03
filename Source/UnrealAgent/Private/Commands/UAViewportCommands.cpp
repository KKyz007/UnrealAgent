// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAViewportCommands.h"
#include "UnrealAgent.h"

#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

TArray<FString> UAViewportCommands::GetSupportedMethods() const
{
	return {
		TEXT("get_viewport_camera"),
		TEXT("move_viewport_camera"),
		TEXT("focus_on_actor"),
	};
}

TSharedPtr<FJsonObject> UAViewportCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("get_viewport_camera"))
	{
		return MakeToolSchema(
			TEXT("get_viewport_camera"),
			TEXT("Get the current editor viewport camera position and rotation")
		);
	}
	else if (MethodName == TEXT("move_viewport_camera"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		for (const FString& Axis : {FString(TEXT("x")), FString(TEXT("y")), FString(TEXT("z"))})
		{
			TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
			Prop->SetStringField(TEXT("type"), TEXT("number"));
			Prop->SetStringField(TEXT("description"), FString::Printf(TEXT("Camera location %s"), *Axis));
			Properties->SetObjectField(FString::Printf(TEXT("location_%s"), *Axis), Prop);
		}

		for (const FString& Comp : {FString(TEXT("pitch")), FString(TEXT("yaw")), FString(TEXT("roll"))})
		{
			TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
			Prop->SetStringField(TEXT("type"), TEXT("number"));
			Prop->SetStringField(TEXT("description"), FString::Printf(TEXT("Camera rotation %s in degrees"), *Comp));
			Properties->SetObjectField(FString::Printf(TEXT("rotation_%s"), *Comp), Prop);
		}

		InputSchema->SetObjectField(TEXT("properties"), Properties);
		return MakeToolSchema(TEXT("move_viewport_camera"), TEXT("Move the editor viewport camera to a specific position and rotation"), InputSchema);
	}
	else if (MethodName == TEXT("focus_on_actor"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> NameProp = MakeShared<FJsonObject>();
		NameProp->SetStringField(TEXT("type"), TEXT("string"));
		NameProp->SetStringField(TEXT("description"), TEXT("Name of the actor to focus the viewport on"));
		Properties->SetObjectField(TEXT("actor_name"), NameProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("actor_name")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("focus_on_actor"), TEXT("Focus the editor viewport camera on a specific actor"), InputSchema);
	}

	return nullptr;
}

bool UAViewportCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("get_viewport_camera")) return ExecuteGetViewportCamera(OutResult, OutError);
	if (MethodName == TEXT("move_viewport_camera")) return ExecuteMoveViewportCamera(Params, OutResult, OutError);
	if (MethodName == TEXT("focus_on_actor")) return ExecuteFocusOnActor(Params, OutResult, OutError);

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAViewportCommands::ExecuteGetViewportCamera(TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	if (!ViewportClient)
	{
		OutError = TEXT("No active viewport client");
		return false;
	}

	FVector Location = ViewportClient->GetViewLocation();
	FRotator Rotation = ViewportClient->GetViewRotation();

	OutResult = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Location.X);
	LocObj->SetNumberField(TEXT("y"), Location.Y);
	LocObj->SetNumberField(TEXT("z"), Location.Z);
	OutResult->SetObjectField(TEXT("location"), LocObj);

	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	RotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	RotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
	OutResult->SetObjectField(TEXT("rotation"), RotObj);

	return true;
}

bool UAViewportCommands::ExecuteMoveViewportCamera(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	if (!ViewportClient)
	{
		OutError = TEXT("No active viewport client");
		return false;
	}

	// Get current values as defaults
	FVector Location = ViewportClient->GetViewLocation();
	FRotator Rotation = ViewportClient->GetViewRotation();

	// Apply location params
	double Temp;
	if (Params->TryGetNumberField(TEXT("location_x"), Temp)) Location.X = Temp;
	if (Params->TryGetNumberField(TEXT("location_y"), Temp)) Location.Y = Temp;
	if (Params->TryGetNumberField(TEXT("location_z"), Temp)) Location.Z = Temp;

	// Also support nested object
	const TSharedPtr<FJsonObject>* LocObj;
	if (Params->TryGetObjectField(TEXT("location"), LocObj))
	{
		(*LocObj)->TryGetNumberField(TEXT("x"), Temp); Location.X = Temp;
		(*LocObj)->TryGetNumberField(TEXT("y"), Temp); Location.Y = Temp;
		(*LocObj)->TryGetNumberField(TEXT("z"), Temp); Location.Z = Temp;
	}

	// Apply rotation params
	if (Params->TryGetNumberField(TEXT("rotation_pitch"), Temp)) Rotation.Pitch = Temp;
	if (Params->TryGetNumberField(TEXT("rotation_yaw"), Temp)) Rotation.Yaw = Temp;
	if (Params->TryGetNumberField(TEXT("rotation_roll"), Temp)) Rotation.Roll = Temp;

	const TSharedPtr<FJsonObject>* RotObj;
	if (Params->TryGetObjectField(TEXT("rotation"), RotObj))
	{
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Temp); Rotation.Pitch = Temp;
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Temp); Rotation.Yaw = Temp;
		(*RotObj)->TryGetNumberField(TEXT("roll"), Temp); Rotation.Roll = Temp;
	}

	ViewportClient->SetViewLocation(Location);
	ViewportClient->SetViewRotation(Rotation);
	ViewportClient->Invalidate();

	OutResult = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> NewLocObj = MakeShared<FJsonObject>();
	NewLocObj->SetNumberField(TEXT("x"), Location.X);
	NewLocObj->SetNumberField(TEXT("y"), Location.Y);
	NewLocObj->SetNumberField(TEXT("z"), Location.Z);
	OutResult->SetObjectField(TEXT("location"), NewLocObj);

	TSharedPtr<FJsonObject> NewRotObj = MakeShared<FJsonObject>();
	NewRotObj->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	NewRotObj->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	NewRotObj->SetNumberField(TEXT("roll"), Rotation.Roll);
	OutResult->SetObjectField(TEXT("rotation"), NewRotObj);

	OutResult->SetBoolField(TEXT("success"), true);

	return true;
}

bool UAViewportCommands::ExecuteFocusOnActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
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

	// Find actor
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

	// Focus viewport on actor
	GEditor->MoveViewportCamerasToActor(*FoundActor, false);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("focused_actor"), FoundActor->GetActorLabel());
	OutResult->SetBoolField(TEXT("success"), true);

	return true;
}
