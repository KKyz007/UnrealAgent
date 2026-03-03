// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commands/UACommandBase.h"

/**
 * Actor manipulation commands.
 * Provides methods to create, update, delete, select, and duplicate actors.
 */
class UAActorCommands : public UACommandBase
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
	bool ExecuteCreateActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
	bool ExecuteDeleteActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);
	bool ExecuteSelectActors(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutResult, FString& OutError);

	/** Helper: Find actor by label or name in the editor world */
	AActor* FindActorByName(const FString& ActorName) const;

	/** Helper: Extract FVector from JSON params */
	FVector ExtractVector(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FVector& Default = FVector::ZeroVector) const;

	/** Helper: Extract FRotator from JSON params */
	FRotator ExtractRotator(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FRotator& Default = FRotator::ZeroRotator) const;
};
