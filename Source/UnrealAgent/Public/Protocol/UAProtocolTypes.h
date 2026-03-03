// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * JSON-RPC 2.0 standard error codes
 */
namespace UAProtocol
{
	// Standard JSON-RPC 2.0 error codes
	constexpr int32 ErrorParseError     = -32700;  // Invalid JSON
	constexpr int32 ErrorInvalidRequest = -32600;  // Not a valid JSON-RPC request
	constexpr int32 ErrorMethodNotFound = -32601;  // Method does not exist
	constexpr int32 ErrorInvalidParams  = -32602;  // Invalid method parameters
	constexpr int32 ErrorInternalError  = -32603;  // Internal error

	// Custom server error codes (-32000 to -32099)
	constexpr int32 ErrorServerError    = -32000;  // Generic server error
	constexpr int32 ErrorExecutionError = -32001;  // Command execution failed
	constexpr int32 ErrorNotConnected   = -32002;  // Not connected to editor

	// JSON-RPC version string
	static const FString JsonRpcVersion = TEXT("2.0");

	// Content-Length header format (LSP-style framing)
	static const FString ContentLengthHeader = TEXT("Content-Length: ");
	static const FString HeaderTerminator = TEXT("\r\n\r\n");
}
