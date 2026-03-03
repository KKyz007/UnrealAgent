// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commands/UACommandBase.h"

/**
 * Project information commands.
 * Provides methods to query project metadata and editor state.
 */
class UAProjectCommands : public UACommandBase
{
public:
	virtual TArray<FString> GetSupportedMethods() const override;
	virtual TSharedPtr<FJsonObject> GetToolSchema(const FString& MethodName) const override;
	virtual bool Execute(
		const FString& MethodName,
		const TSharedPtr<FJsonObject>& Params,
		TSharedPtr<FJsonObject>& OutResult,
		FString& OutError
	) override;

private:
	/** Get project name, engine version, modules, plugins */
	bool ExecuteGetProjectInfo(TSharedPtr<FJsonObject>& OutResult, FString& OutError);

	/** Get current editor state (level, PIE status, selection) */
	bool ExecuteGetEditorState(TSharedPtr<FJsonObject>& OutResult, FString& OutError);
};
