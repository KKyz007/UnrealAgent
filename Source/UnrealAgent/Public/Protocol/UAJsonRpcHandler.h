// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UACommandRegistry;

/**
 * Handles JSON-RPC 2.0 request parsing, dispatch, and response construction.
 */
class UAJsonRpcHandler
{
public:
	UAJsonRpcHandler(TSharedPtr<UACommandRegistry> InCommandRegistry);

	/**
	 * Process a JSON-RPC request string and return a response string.
	 * @param JsonString  The raw JSON-RPC request
	 * @return JSON-RPC response string
	 */
	FString HandleRequest(const FString& JsonString);

private:
	/** Build a JSON-RPC success response */
	FString MakeSuccessResponse(TSharedPtr<FJsonValue> Id, TSharedPtr<FJsonObject> Result);

	/** Build a JSON-RPC error response */
	FString MakeErrorResponse(TSharedPtr<FJsonValue> Id, int32 ErrorCode, const FString& Message);

	/** Serialize a JSON object to string */
	static FString JsonToString(const TSharedRef<FJsonObject>& JsonObject);

	/** Command registry for dispatching method calls */
	TSharedPtr<UACommandRegistry> CommandRegistry;
};
