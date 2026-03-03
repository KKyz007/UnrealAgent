// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Abstract base class for all command groups.
 * Each subclass represents a group of related tool commands.
 */
class UACommandBase : public TSharedFromThis<UACommandBase>
{
public:
	virtual ~UACommandBase() = default;

	/** Return the list of method names this command group supports */
	virtual TArray<FString> GetSupportedMethods() const = 0;

	/** Return the JSON schema for a specific method (for MCP tools/list) */
	virtual TSharedPtr<FJsonObject> GetToolSchema(const FString& MethodName) const = 0;

	/**
	 * Execute a command.
	 * @param MethodName  The method to execute (e.g., "list_assets")
	 * @param Params      Request parameters as JSON object
	 * @param OutResult   Output result JSON object
	 * @param OutError    Output error message (if returning false)
	 * @return true on success, false on failure
	 */
	virtual bool Execute(
		const FString& MethodName,
		const TSharedPtr<FJsonObject>& Params,
		TSharedPtr<FJsonObject>& OutResult,
		FString& OutError
	) = 0;

protected:
	/** Helper: Create a simple tool schema JSON object */
	TSharedPtr<FJsonObject> MakeToolSchema(
		const FString& Name,
		const FString& Description,
		TSharedPtr<FJsonObject> InputSchema = nullptr
	) const;
};
