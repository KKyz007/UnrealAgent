// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UACommandBase;

/**
 * Central registry for all command groups.
 * Maps method names to their handlers and provides dispatch functionality.
 */
class UACommandRegistry
{
public:
	UACommandRegistry();

	/** Register a command group handler */
	void RegisterCommand(TSharedPtr<UACommandBase> Command);

	/**
	 * Dispatch a method call to the appropriate handler.
	 * @return true on success, false on failure (OutError set)
	 */
	bool Dispatch(
		const FString& MethodName,
		const TSharedPtr<FJsonObject>& Params,
		TSharedPtr<FJsonObject>& OutResult,
		FString& OutError
	);

	/** Get JSON schemas for all registered tools (for list_tools) */
	TArray<TSharedPtr<FJsonObject>> GetAllToolSchemas() const;

	/** Handle the built-in list_tools method */
	TSharedPtr<FJsonObject> HandleListTools();

private:
	/** Method name -> Command handler mapping */
	TMap<FString, TSharedPtr<UACommandBase>> MethodMap;

	/** All registered command groups */
	TArray<TSharedPtr<UACommandBase>> CommandGroups;
};
