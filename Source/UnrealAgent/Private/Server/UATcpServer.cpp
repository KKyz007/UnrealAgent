// Copyright KuoYu. All Rights Reserved.

#include "Server/UATcpServer.h"
#include "Server/UAClientConnection.h"
#include "Commands/UACommandRegistry.h"
#include "Settings/UASettings.h"
#include "UnrealAgent.h"

#include "Common/TcpListener.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"

UATcpServer::UATcpServer()
{
	CommandRegistry = MakeShared<UACommandRegistry>();
}

UATcpServer::~UATcpServer()
{
	Stop();
}

bool UATcpServer::Start(const FString& BindAddress, int32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogUnrealAgent, Warning, TEXT("TCP server is already running"));
		return true;
	}

	// Parse the bind address
	FIPv4Address Address;
	if (!FIPv4Address::Parse(BindAddress, Address))
	{
		UE_LOG(LogUnrealAgent, Error, TEXT("Invalid bind address: %s"), *BindAddress);
		return false;
	}

	FIPv4Endpoint Endpoint(Address, Port);

	// Create the TCP listener
	Listener = MakeUnique<FTcpListener>(Endpoint, FTimespan::FromSeconds(1.0));
	Listener->OnConnectionAccepted().BindRaw(this, &UATcpServer::OnConnectionAccepted);

	if (!Listener->Init())
	{
		UE_LOG(LogUnrealAgent, Error, TEXT("Failed to initialize TCP listener on %s:%d"), *BindAddress, Port);
		Listener.Reset();
		return false;
	}

	// Register game thread ticker for processing client messages
	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &UATcpServer::Tick),
		0.01f  // ~100 Hz tick rate
	);

	ListenPort = Port;
	bIsRunning = true;

	UE_LOG(LogUnrealAgent, Log, TEXT("TCP server listening on %s:%d"), *BindAddress, Port);
	return true;
}

void UATcpServer::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	bIsRunning = false;

	// Remove ticker
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}

	// Close all connections
	for (auto& Connection : Connections)
	{
		if (Connection.IsValid())
		{
			Connection->Close();
		}
	}
	Connections.Empty();

	// Stop listener
	if (Listener.IsValid())
	{
		Listener.Reset();
	}

	UE_LOG(LogUnrealAgent, Log, TEXT("TCP server stopped"));
}

bool UATcpServer::IsRunning() const
{
	return bIsRunning;
}

bool UATcpServer::OnConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint)
{
	if (!bIsRunning)
	{
		return false;
	}

	const UUASettings* Settings = GetDefault<UUASettings>();

	// First, do a quick cleanup of dead connections before checking limit
	for (int32 i = Connections.Num() - 1; i >= 0; --i)
	{
		if (!Connections[i].IsValid() || !Connections[i]->IsConnected())
		{
			UE_LOG(LogUnrealAgent, Log, TEXT("Cleaning up stale connection: %s"),
				Connections[i].IsValid() ? *Connections[i]->GetEndpointStr() : TEXT("invalid"));
			if (Connections[i].IsValid())
			{
				Connections[i]->Close();
			}
			Connections.RemoveAt(i);
		}
	}

	// Check connection limit after cleanup
	if (Connections.Num() >= Settings->MaxConnections)
	{
		UE_LOG(LogUnrealAgent, Warning, TEXT("Connection rejected from %s - max connections reached (%d)"),
			*ClientEndpoint.ToString(), Settings->MaxConnections);
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
		return true;
	}

	FString EndpointStr = ClientEndpoint.ToString();
	UE_LOG(LogUnrealAgent, Log, TEXT("New connection from %s"), *EndpointStr);

	TSharedPtr<UAClientConnection> NewConnection = MakeShared<UAClientConnection>(
		ClientSocket, EndpointStr, CommandRegistry);
	Connections.Add(NewConnection);

	return true;
}

bool UATcpServer::Tick(float DeltaTime)
{
	if (!bIsRunning)
	{
		return false;
	}

	// Process pending data for all connections, remove disconnected ones
	for (int32 i = Connections.Num() - 1; i >= 0; --i)
	{
		if (!Connections[i].IsValid() || !Connections[i]->IsConnected() || !Connections[i]->ProcessPendingData())
		{
			if (Connections[i].IsValid())
			{
				UE_LOG(LogUnrealAgent, Log, TEXT("Connection closed: %s"), *Connections[i]->GetEndpointStr());
				Connections[i]->Close();
			}
			Connections.RemoveAt(i);
		}
	}

	return true;  // Continue ticking
}
