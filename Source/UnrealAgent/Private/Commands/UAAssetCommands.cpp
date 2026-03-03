// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAAssetCommands.h"
#include "UnrealAgent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"

TArray<FString> UAAssetCommands::GetSupportedMethods() const
{
	return {
		TEXT("list_assets"),
		TEXT("search_assets"),
		TEXT("get_asset_info"),
		TEXT("get_asset_references"),
	};
}

TSharedPtr<FJsonObject> UAAssetCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("list_assets"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathProp = MakeShared<FJsonObject>();
		PathProp->SetStringField(TEXT("type"), TEXT("string"));
		PathProp->SetStringField(TEXT("description"), TEXT("Asset path to list (e.g., /Game/Blueprints). Defaults to /Game"));
		Properties->SetObjectField(TEXT("path"), PathProp);

		TSharedPtr<FJsonObject> ClassProp = MakeShared<FJsonObject>();
		ClassProp->SetStringField(TEXT("type"), TEXT("string"));
		ClassProp->SetStringField(TEXT("description"), TEXT("Filter by class name (e.g., Blueprint, StaticMesh, Material)"));
		Properties->SetObjectField(TEXT("class_filter"), ClassProp);

		TSharedPtr<FJsonObject> RecursiveProp = MakeShared<FJsonObject>();
		RecursiveProp->SetStringField(TEXT("type"), TEXT("boolean"));
		RecursiveProp->SetStringField(TEXT("description"), TEXT("Search recursively in subdirectories. Defaults to true"));
		Properties->SetObjectField(TEXT("recursive"), RecursiveProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);
		return MakeToolSchema(TEXT("list_assets"), TEXT("List assets in the project, optionally filtered by path and class"), InputSchema);
	}
	else if (MethodName == TEXT("search_assets"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> QueryProp = MakeShared<FJsonObject>();
		QueryProp->SetStringField(TEXT("type"), TEXT("string"));
		QueryProp->SetStringField(TEXT("description"), TEXT("Search query string to match against asset names"));
		Properties->SetObjectField(TEXT("query"), QueryProp);

		TSharedPtr<FJsonObject> ClassProp = MakeShared<FJsonObject>();
		ClassProp->SetStringField(TEXT("type"), TEXT("string"));
		ClassProp->SetStringField(TEXT("description"), TEXT("Filter by class name"));
		Properties->SetObjectField(TEXT("class_filter"), ClassProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("query")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("search_assets"), TEXT("Search for assets by name with optional class filter"), InputSchema);
	}
	else if (MethodName == TEXT("get_asset_info"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathProp = MakeShared<FJsonObject>();
		PathProp->SetStringField(TEXT("type"), TEXT("string"));
		PathProp->SetStringField(TEXT("description"), TEXT("Full asset path (e.g., /Game/Blueprints/BP_Player)"));
		Properties->SetObjectField(TEXT("asset_path"), PathProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("asset_path")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("get_asset_info"), TEXT("Get detailed information about a specific asset"), InputSchema);
	}
	else if (MethodName == TEXT("get_asset_references"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathProp = MakeShared<FJsonObject>();
		PathProp->SetStringField(TEXT("type"), TEXT("string"));
		PathProp->SetStringField(TEXT("description"), TEXT("Full asset path to query references for"));
		Properties->SetObjectField(TEXT("asset_path"), PathProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("asset_path")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(TEXT("get_asset_references"), TEXT("Get referencers and dependencies of an asset"), InputSchema);
	}

	return nullptr;
}

bool UAAssetCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("list_assets")) return ExecuteListAssets(Params, OutResult, OutError);
	if (MethodName == TEXT("search_assets")) return ExecuteSearchAssets(Params, OutResult, OutError);
	if (MethodName == TEXT("get_asset_info")) return ExecuteGetAssetInfo(Params, OutResult, OutError);
	if (MethodName == TEXT("get_asset_references")) return ExecuteGetAssetReferences(Params, OutResult, OutError);

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAAssetCommands::ExecuteListAssets(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FString Path = TEXT("/Game");
	Params->TryGetStringField(TEXT("path"), Path);

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(FName(*Path), Assets, bRecursive);

	TArray<TSharedPtr<FJsonValue>> AssetList;
	for (const FAssetData& Asset : Assets)
	{
		// Apply class filter if specified
		if (!ClassFilter.IsEmpty())
		{
			FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
			if (!AssetClassName.Contains(ClassFilter))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());
		AssetObj->SetStringField(TEXT("package"), Asset.PackageName.ToString());

		AssetList.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetArrayField(TEXT("assets"), AssetList);
	OutResult->SetNumberField(TEXT("count"), AssetList.Num());
	OutResult->SetStringField(TEXT("path"), Path);

	return true;
}

bool UAAssetCommands::ExecuteSearchAssets(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
	{
		OutError = TEXT("Invalid params: 'query' is required");
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	// Get all assets under /Game
	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game")), AllAssets, true);

	TArray<TSharedPtr<FJsonValue>> MatchedAssets;
	for (const FAssetData& Asset : AllAssets)
	{
		// Name filter
		FString AssetName = Asset.AssetName.ToString();
		if (!AssetName.Contains(Query, ESearchCase::IgnoreCase))
		{
			continue;
		}

		// Class filter
		if (!ClassFilter.IsEmpty())
		{
			FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
			if (!AssetClassName.Contains(ClassFilter))
			{
				continue;
			}
		}

		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), AssetName);
		AssetObj->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.GetAssetName().ToString());

		MatchedAssets.Add(MakeShared<FJsonValueObject>(AssetObj));
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetArrayField(TEXT("assets"), MatchedAssets);
	OutResult->SetNumberField(TEXT("count"), MatchedAssets.Num());
	OutResult->SetStringField(TEXT("query"), Query);

	return true;
}

bool UAAssetCommands::ExecuteGetAssetInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		OutError = TEXT("Invalid params: 'asset_path' is required");
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
	if (!AssetData.IsValid())
	{
		OutError = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
		return false;
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	OutResult->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
	OutResult->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	OutResult->SetStringField(TEXT("package"), AssetData.PackageName.ToString());

	// Tags (metadata)
	TSharedPtr<FJsonObject> TagsObj = MakeShared<FJsonObject>();
	AssetData.EnumerateTags([&TagsObj](TPair<FName, FAssetTagValueRef> Tag)
	{
		TagsObj->SetStringField(Tag.Key.ToString(), Tag.Value.AsString());
	});
	OutResult->SetObjectField(TEXT("tags"), TagsObj);

	return true;
}

bool UAAssetCommands::ExecuteGetAssetReferences(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		OutError = TEXT("Invalid params: 'asset_path' is required");
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FName PackageName = FName(*FPackageName::ObjectPathToPackageName(AssetPath));

	// Get referencers (what references this asset)
	TArray<FName> Referencers;
	AssetRegistry.GetReferencers(PackageName, Referencers);

	TArray<TSharedPtr<FJsonValue>> ReferencerList;
	for (const FName& Ref : Referencers)
	{
		ReferencerList.Add(MakeShared<FJsonValueString>(Ref.ToString()));
	}

	// Get dependencies (what this asset depends on)
	TArray<FName> Dependencies;
	AssetRegistry.GetDependencies(PackageName, Dependencies);

	TArray<TSharedPtr<FJsonValue>> DependencyList;
	for (const FName& Dep : Dependencies)
	{
		DependencyList.Add(MakeShared<FJsonValueString>(Dep.ToString()));
	}

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetStringField(TEXT("asset_path"), AssetPath);
	OutResult->SetArrayField(TEXT("referencers"), ReferencerList);
	OutResult->SetNumberField(TEXT("referencer_count"), ReferencerList.Num());
	OutResult->SetArrayField(TEXT("dependencies"), DependencyList);
	OutResult->SetNumberField(TEXT("dependency_count"), DependencyList.Num());

	return true;
}
