// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

class UAClientConnection;
class UACommandRegistry;

/**
 * TCP server that listens for incoming JSON-RPC connections.
 * Runs on the game thread via FTSTicker for safe UE API access.
 */
class UATcpServer : public TSharedFromThis<UATcpServer>
{
public:
	UATcpServer();
	~UATcpServer();

	/** Start listening on the specified address and port */
	bool Start(const FString& BindAddress, int32 Port);

	/** Stop the server and close all connections */
	void Stop();

	/** Check if the server is currently running */
	bool IsRunning() const;

	/** Get the command registry (shared across all connections) */
	TSharedPtr<UACommandRegistry> GetCommandRegistry() const { return CommandRegistry; }

private:
	/** Callback when a new TCP connection is accepted */
	bool OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	/** Game thread tick - process pending client data */
	bool Tick(float DeltaTime);

	/** TCP listener instance */
	TUniquePtr<class FTcpListener> Listener;

	/** Active client connections */
	TArray<TSharedPtr<UAClientConnection>> Connections;

	/** Ticker delegate handle for game thread processing */
	FTSTicker::FDelegateHandle TickDelegateHandle;

	/** Command registry shared across all connections */
	TSharedPtr<UACommandRegistry> CommandRegistry;

	/** Server state */
	bool bIsRunning = false;
	int32 ListenPort = 0;
};
