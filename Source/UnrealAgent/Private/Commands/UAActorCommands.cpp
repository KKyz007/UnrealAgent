// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAActorCommands.h"
#include "UnrealAgent.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "UObject/UObjectGlobals.h"

TArray<FString> UAActorCommands::GetSupportedMethods() const
{
	return {
		TEXT("create_actor"),
		TEXT("delete_actor"),
		TEXT("select_actors"),
	};
}

TSharedPtr<FJsonObject> UAActorCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("create_actor"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> ClassProp = MakeShared<FJsonObject>();
		ClassProp->SetStringField(TEXT("type"), TEXT("string"));
		ClassProp->SetStringField(TEXT("description"), TEXT("Actor class to create (e.g., StaticMeshActor, PointLight, CameraActor, DirectionalLight, SpotLight)"));
		Properties->SetObjectField(TEXT("class_name"), ClassProp);

		TSharedPtr<FJsonObject> LabelProp = MakeShared<FJsonObject>();
		LabelProp->SetStringField(TEXT("type"), TEXT("string"));
		LabelProp->SetStringField(TEXT("description"), TEXT("Display label for the new actor"));
		Properties->SetObjectField(TEXT("label"), LabelProp);

		// Location
		auto AddVectorProp = [&Properties](const FString& Prefix, const FString& Desc)
		{
			for (const FString& Axis : {FString(TEXT("x")), FString(TEXT("y")), FString(TEXT("z"))})
			{
				TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
				Prop->SetStringField(TEXT("type"), TEXT("number"));
				Prop->SetStringField(TEXT("description"), FString::Printf(TEXT("%s %s coordinate"), *Desc, *Axis));
				Properties->SetObjectField(FString::Printf(TEXT("%s_%s"), *Prefix, *Axis), Prop);
			}
		};

		AddVectorProp(TEXT("location"), TEXT("World position"));
		AddVectorProp(TEXT("rotation"), TEXT("Rotation"));
		AddVectorProp(TEXT("scale"), TEXT("Scale"));

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("class_name")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("create_actor"), TEXT("Create a new actor in the current level"), InputSchema);
	}
	else if (MethodName == TEXT("delete_actor"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> NameProp = MakeShared<FJsonObject>();
		NameProp->SetStringField(TEXT("type"), TEXT("string"));
		NameProp->SetStringField(TEXT("description"), TEXT("Name or label of the actor to delete"));
		Properties->SetObjectField(TEXT("actor_name"), NameProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("actor_name")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("delete_actor"), TEXT("Delete an actor from the current level"), InputSchema);
	}
	else if (MethodName == TEXT("select_actors"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> NamesProp = MakeShared<FJsonObject>();
		NamesProp->SetStringField(TEXT("type"), TEXT("array"));
		NamesProp->SetStringField(TEXT("description"), TEXT("Array of actor names to select. Empty array clears selection."));
		Properties->SetObjectField(TEXT("actor_names"), NamesProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("actor_names")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("select_actors"), TEXT("Select specific actors in the editor"), InputSchema);
	}

	return nullptr;
}

bool UAActorCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("create_actor")) return ExecuteCreateActor(Params, OutResult, OutError);
	if (MethodName == TEXT("delete_actor")) return ExecuteDeleteActor(Params, OutResult, OutError);
	if (MethodName == TEXT("select_actors")) return ExecuteSelectActors(Params, OutResult, OutError);

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAActorCommands::ExecuteCreateActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString ClassName;
	if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
	{
		OutError = TEXT("Invalid params: 'class_name' is required");
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

	// Map common class names to UClass
	UClass* ActorClass = nullptr;

	// Try to find the class by name
	static TMap<FString, UClass*> ClassMap = {
		{TEXT("StaticMeshActor"), AStaticMeshActor::StaticClass()},
		{TEXT("PointLight"), APointLight::StaticClass()},
		{TEXT("DirectionalLight"), ADirectionalLight::StaticClass()},
		{TEXT("SpotLight"), ASpotLight::StaticClass()},
		{TEXT("CameraActor"), ACameraActor::StaticClass()},
	};

	if (UClass** Found = ClassMap.Find(ClassName))
	{
		ActorClass = *Found;
	}
	else
	{
		// Try to find class dynamically
		ActorClass = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::ExactClass);
		if (!ActorClass)
		{
			// Try with 'A' prefix (Unreal convention for Actor classes)
			ActorClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *ClassName), EFindFirstObjectOptions::ExactClass);
		}
	}

	if (!ActorClass)
	{
		OutError = FString::Printf(TEXT("Actor class not found: %s"), *ClassName);
		return false;
	}

	// Extract transform
	FVector Location = ExtractVector(Params, TEXT("location"));
	FRotator Rotation = ExtractRotator(Params, TEXT("rotation"));
	FVector Scale = ExtractVector(Params, TEXT("scale"), FVector::OneVector);

	// Also support flat params (location_x, location_y, etc.)
	double Temp;
	if (Params->TryGetNumberField(TEXT("location_x"), Temp)) Location.X = Temp;
	if (Params->TryGetNumberField(TEXT("location_y"), Temp)) Location.Y = Temp;
	if (Params->TryGetNumberField(TEXT("location_z"), Temp)) Location.Z = Temp;
	if (Params->TryGetNumberField(TEXT("rotation_pitch"), Temp)) Rotation.Pitch = Temp;
	if (Params->TryGetNumberField(TEXT("rotation_yaw"), Temp)) Rotation.Yaw = Temp;
	if (Params->TryGetNumberField(TEXT("rotation_roll"), Temp)) Rotation.Roll = Temp;
	if (Params->TryGetNumberField(TEXT("scale_x"), Temp)) Scale.X = Temp;
	if (Params->TryGetNumberField(TEXT("scale_y"), Temp)) Scale.Y = Temp;
	if (Params->TryGetNumberField(TEXT("scale_z"), Temp)) Scale.Z = Temp;

	// Spawn the actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	if (!NewActor)
	{
		OutError = FString::Printf(TEXT("Failed to spawn actor of class %s"), *ClassName);
		return false;
	}

	// Set scale
	if (Scale != FVector::OneVector)
	{
		NewActor->SetActorScale3D(Scale);
	}

	// Set label
	FString Label;
	if (Params->TryGetStringField(TEXT("label"), Label) && !Label.IsEmpty())
	{
		NewActor->SetActorLabel(Label);
	}

	UE_LOG(LogUnrealAgent, Log, TEXT("Created actor: %s (%s) at %s"),
		*NewActor->GetActorLabel(), *ClassName, *Location.ToString());

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("name"), NewActor->GetActorLabel());
	OutResult->SetStringField(TEXT("class"), ClassName);

	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Location.X);
	LocObj->SetNumberField(TEXT("y"), Location.Y);
	LocObj->SetNumberField(TEXT("z"), Location.Z);
	OutResult->SetObjectField(TEXT("location"), LocObj);

	OutResult->SetBoolField(TEXT("success"), true);

	return true;
}

bool UAActorCommands::ExecuteDeleteActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString ActorName;
	if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
	{
		OutError = TEXT("Invalid params: 'actor_name' is required");
		return false;
	}

	AActor* Actor = FindActorByName(ActorName);
	if (!Actor)
	{
		OutError = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
		return false;
	}

	FString DeletedName = Actor->GetActorLabel();
	FString DeletedClass = Actor->GetClass()->GetName();

	// Deselect the actor before destroying to prevent USelection from
	// holding a stale reference, which causes a TypedElement assert crash
	if (GEditor)
	{
		GEditor->SelectActor(Actor, /*bInSelected=*/false, /*bNotify=*/true);
	}

	if (!Actor->Destroy())
	{
		OutError = FString::Printf(TEXT("Failed to destroy actor: %s"), *ActorName);
		return false;
	}

	UE_LOG(LogUnrealAgent, Log, TEXT("Deleted actor: %s (%s)"), *DeletedName, *DeletedClass);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("deleted_actor"), DeletedName);
	OutResult->SetStringField(TEXT("class"), DeletedClass);
	OutResult->SetBoolField(TEXT("success"), true);

	return true;
}

bool UAActorCommands::ExecuteSelectActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* NamesArray;
	if (!Params->TryGetArrayField(TEXT("actor_names"), NamesArray))
	{
		OutError = TEXT("Invalid params: 'actor_names' array is required");
		return false;
	}

	// Clear current selection
	GEditor->SelectNone(true, true, false);

	TArray<TSharedPtr<FJsonValue>> SelectedList;
	TArray<FString> NotFoundList;

	for (const auto& NameValue : *NamesArray)
	{
		FString Name = NameValue->AsString();
		AActor* Actor = FindActorByName(Name);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, true);
			SelectedList.Add(MakeShared<FJsonValueString>(Name));
		}
		else
		{
			NotFoundList.Add(Name);
		}
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetArrayField(TEXT("selected"), SelectedList);
	OutResult->SetNumberField(TEXT("selected_count"), SelectedList.Num());

	if (NotFoundList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFound;
		for (const FString& Name : NotFoundList)
		{
			NotFound.Add(MakeShared<FJsonValueString>(Name));
		}
		OutResult->SetArrayField(TEXT("not_found"), NotFound);
	}

	return true;
}

AActor* UAActorCommands::FindActorByName(const FString& ActorName) const
{
	if (!GEditor)
	{
		return nullptr;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((*It)->GetActorLabel() == ActorName || (*It)->GetName() == ActorName)
		{
			return *It;
		}
	}

	return nullptr;
}

FVector UAActorCommands::ExtractVector(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FVector& Default) const
{
	const TSharedPtr<FJsonObject>* VecObj;
	if (JsonObj->TryGetObjectField(FieldName, VecObj))
	{
		double X = Default.X, Y = Default.Y, Z = Default.Z;
		(*VecObj)->TryGetNumberField(TEXT("x"), X);
		(*VecObj)->TryGetNumberField(TEXT("y"), Y);
		(*VecObj)->TryGetNumberField(TEXT("z"), Z);
		return FVector(X, Y, Z);
	}
	return Default;
}

FRotator UAActorCommands::ExtractRotator(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FRotator& Default) const
{
	const TSharedPtr<FJsonObject>* RotObj;
	if (JsonObj->TryGetObjectField(FieldName, RotObj))
	{
		double Pitch = Default.Pitch, Yaw = Default.Yaw, Roll = Default.Roll;
		(*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
		(*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
		(*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
		return FRotator(Pitch, Yaw, Roll);
	}
	return Default;
}
