// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FSocket;
class UAJsonRpcHandler;

/**
 * Manages a single TCP client connection.
 * Handles message framing (Content-Length protocol) and dispatches
 * complete JSON-RPC messages to the handler.
 */
class UAClientConnection : public TSharedFromThis<UAClientConnection>
{
public:
	UAClientConnection(FSocket* InSocket, const FString& InEndpointStr, TSharedPtr<class UACommandRegistry> InCommandRegistry);
	~UAClientConnection();

	/** Check if the connection is still alive */
	bool IsConnected() const;

	/** Read and process pending data. Returns false if connection is broken. */
	bool ProcessPendingData();

	/** Close the connection */
	void Close();

	/** Get endpoint string for logging */
	const FString& GetEndpointStr() const { return EndpointStr; }

private:
	/** Try to extract a complete message from the receive buffer */
	bool TryExtractMessage(FString& OutMessage);

	/** Handle a complete JSON-RPC message */
	void HandleMessage(const FString& Message);

	/** Send a response string back to the client */
	void SendResponse(const FString& Response);

	/** The underlying socket */
	FSocket* Socket;

	/** Remote endpoint string (for logging) */
	FString EndpointStr;

	/** Receive buffer for accumulating partial data */
	FString ReceiveBuffer;

	/** JSON-RPC request handler */
	TSharedPtr<UAJsonRpcHandler> RpcHandler;
};
