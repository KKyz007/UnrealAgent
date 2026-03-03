// Copyright KuoYu. All Rights Reserved.

#include "Protocol/UAJsonRpcHandler.h"
#include "Protocol/UAProtocolTypes.h"
#include "Commands/UACommandRegistry.h"
#include "UnrealAgent.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

UAJsonRpcHandler::UAJsonRpcHandler(TSharedPtr<UACommandRegistry> InCommandRegistry)
	: CommandRegistry(InCommandRegistry)
{
}

FString UAJsonRpcHandler::HandleRequest(const FString& JsonString)
{
	// Parse JSON
	TSharedPtr<FJsonObject> RequestObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
	{
		UE_LOG(LogUnrealAgent, Warning, TEXT("Failed to parse JSON-RPC request"));
		return MakeErrorResponse(nullptr, UAProtocol::ErrorParseError, TEXT("Parse error"));
	}

	// Validate JSON-RPC 2.0 format
	FString JsonRpc;
	if (!RequestObj->TryGetStringField(TEXT("jsonrpc"), JsonRpc) || JsonRpc != TEXT("2.0"))
	{
		return MakeErrorResponse(nullptr, UAProtocol::ErrorInvalidRequest, TEXT("Invalid JSON-RPC version"));
	}

	// Extract id (can be string, number, or null)
	TSharedPtr<FJsonValue> Id = RequestObj->TryGetField(TEXT("id"));

	// Extract method
	FString Method;
	if (!RequestObj->TryGetStringField(TEXT("method"), Method))
	{
		return MakeErrorResponse(Id, UAProtocol::ErrorInvalidRequest, TEXT("Missing method field"));
	}

	// Extract params (default to empty object)
	TSharedPtr<FJsonObject> Params;
	const TSharedPtr<FJsonObject>* ParamsPtr;
	if (RequestObj->TryGetObjectField(TEXT("params"), ParamsPtr))
	{
		Params = *ParamsPtr;
	}
	else
	{
		Params = MakeShared<FJsonObject>();
	}

	UE_LOG(LogUnrealAgent, Log, TEXT("JSON-RPC call: %s"), *Method);

	// Handle built-in methods
	if (Method == TEXT("list_tools"))
	{
		TSharedPtr<FJsonObject> Result = CommandRegistry->HandleListTools();
		return MakeSuccessResponse(Id, Result);
	}

	// Dispatch to command registry
	TSharedPtr<FJsonObject> Result;
	FString ErrorMessage;

	if (CommandRegistry->Dispatch(Method, Params, Result, ErrorMessage))
	{
		return MakeSuccessResponse(Id, Result);
	}
	else
	{
		// Determine error code based on message
		int32 ErrorCode = UAProtocol::ErrorExecutionError;
		if (ErrorMessage.Contains(TEXT("not found")) || ErrorMessage.Contains(TEXT("Unknown method")))
		{
			ErrorCode = UAProtocol::ErrorMethodNotFound;
		}
		else if (ErrorMessage.Contains(TEXT("Invalid param")))
		{
			ErrorCode = UAProtocol::ErrorInvalidParams;
		}

		return MakeErrorResponse(Id, ErrorCode, ErrorMessage);
	}
}

FString UAJsonRpcHandler::MakeSuccessResponse(TSharedPtr<FJsonValue> Id, TSharedPtr<FJsonObject> Result)
{
	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), UAProtocol::JsonRpcVersion);

	if (Id.IsValid())
	{
		Response->SetField(TEXT("id"), Id);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	if (Result.IsValid())
	{
		Response->SetObjectField(TEXT("result"), Result);
	}
	else
	{
		Response->SetObjectField(TEXT("result"), MakeShared<FJsonObject>());
	}

	return JsonToString(Response);
}

FString UAJsonRpcHandler::MakeErrorResponse(TSharedPtr<FJsonValue> Id, int32 ErrorCode, const FString& Message)
{
	TSharedRef<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), ErrorCode);
	ErrorObj->SetStringField(TEXT("message"), Message);

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), UAProtocol::JsonRpcVersion);

	if (Id.IsValid())
	{
		Response->SetField(TEXT("id"), Id);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	Response->SetObjectField(TEXT("error"), ErrorObj);

	return JsonToString(Response);
}

FString UAJsonRpcHandler::JsonToString(const TSharedRef<FJsonObject>& JsonObject)
{
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject, Writer);
	return OutputString;
}
