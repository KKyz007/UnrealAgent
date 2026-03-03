// Copyright KuoYu. All Rights Reserved.

#include "Server/UAClientConnection.h"
#include "Protocol/UAJsonRpcHandler.h"
#include "Protocol/UAProtocolTypes.h"
#include "Commands/UACommandRegistry.h"
#include "UnrealAgent.h"

#include "Sockets.h"
#include "SocketSubsystem.h"

UAClientConnection::UAClientConnection(FSocket* InSocket, const FString& InEndpointStr, TSharedPtr<UACommandRegistry> InCommandRegistry)
	: Socket(InSocket)
	, EndpointStr(InEndpointStr)
{
	RpcHandler = MakeShared<UAJsonRpcHandler>(InCommandRegistry);

	// Configure socket for non-blocking reads
	if (Socket)
	{
		Socket->SetNonBlocking(true);
	}
}

UAClientConnection::~UAClientConnection()
{
	Close();
}

bool UAClientConnection::IsConnected() const
{
	if (!Socket)
	{
		return false;
	}

	// For non-blocking sockets, GetConnectionState can be unreliable
	// after remote disconnect. Use Wait() to check socket readability
	// as a more reliable indicator.
	ESocketConnectionState State = Socket->GetConnectionState();
	return State == SCS_Connected;
}

bool UAClientConnection::ProcessPendingData()
{
	if (!Socket)
	{
		return false;
	}

	// Use Wait to probe socket status - more reliable than HasPendingData
	// for detecting disconnected peers on non-blocking sockets
	Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero());

	// Check for socket errors by verifying connection state
	if (Socket->GetConnectionState() == SCS_ConnectionError)
	{
		UE_LOG(LogUnrealAgent, Log, TEXT("Socket error detected for %s"), *EndpointStr);
		return false;
	}

	// Check if there's data available
	uint32 PendingDataSize = 0;
	if (!Socket->HasPendingData(PendingDataSize) || PendingDataSize == 0)
	{
		// No data available - probe with a zero-byte recv to detect closed connections
		uint8 ProbeBuf;
		int32 ProbeRead = 0;
		// Peek at the socket to see if the connection is still alive
		if (!Socket->Recv(&ProbeBuf, 1, ProbeRead, ESocketReceiveFlags::Peek))
		{
			// Recv failed = connection dead
			return false;
		}
		if (ProbeRead == 0 && Socket->GetConnectionState() != SCS_Connected)
		{
			return false;
		}
		return true;
	}

	// Read available data
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(FMath::Min(PendingDataSize, (uint32)65536));

	int32 BytesRead = 0;
	if (!Socket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead))
	{
		UE_LOG(LogUnrealAgent, Warning, TEXT("Failed to read from %s"), *EndpointStr);
		return false;
	}

	if (BytesRead <= 0)
	{
		// Connection closed by remote
		return false;
	}

	// Append to receive buffer
	FString ReceivedStr = FString(BytesRead, UTF8_TO_TCHAR((const ANSICHAR*)Buffer.GetData()));
	ReceiveBuffer += ReceivedStr;

	// Try to extract complete messages
	FString Message;
	while (TryExtractMessage(Message))
	{
		HandleMessage(Message);
	}

	return true;
}

bool UAClientConnection::TryExtractMessage(FString& OutMessage)
{
	// Look for Content-Length header in the buffer
	// Format: "Content-Length: <N>\r\n\r\n<payload>"

	static const FString ContentLengthPrefix = TEXT("Content-Length: ");

	int32 HeaderStart = ReceiveBuffer.Find(ContentLengthPrefix);
	if (HeaderStart == INDEX_NONE)
	{
		return false;
	}

	// Find the end of the header line
	int32 HeaderEnd = ReceiveBuffer.Find(TEXT("\r\n\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HeaderStart);
	if (HeaderEnd == INDEX_NONE)
	{
		return false;
	}

	// Extract content length value
	int32 LengthStart = HeaderStart + ContentLengthPrefix.Len();
	int32 LengthEnd = ReceiveBuffer.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LengthStart);
	if (LengthEnd == INDEX_NONE)
	{
		return false;
	}

	FString LengthStr = ReceiveBuffer.Mid(LengthStart, LengthEnd - LengthStart);
	int32 ContentLength = FCString::Atoi(*LengthStr);
	if (ContentLength <= 0)
	{
		// Invalid content length, discard this header
		ReceiveBuffer = ReceiveBuffer.Mid(HeaderEnd + 4);
		return false;
	}

	// Check if we have the complete payload
	int32 PayloadStart = HeaderEnd + 4;  // After "\r\n\r\n"
	int32 RemainingLength = ReceiveBuffer.Len() - PayloadStart;
	if (RemainingLength < ContentLength)
	{
		// Not enough data yet
		return false;
	}

	// Extract the complete message
	OutMessage = ReceiveBuffer.Mid(PayloadStart, ContentLength);

	// Remove the processed data from the buffer
	ReceiveBuffer = ReceiveBuffer.Mid(PayloadStart + ContentLength);

	return true;
}

void UAClientConnection::HandleMessage(const FString& Message)
{
	UE_LOG(LogUnrealAgent, Verbose, TEXT("[%s] Received: %s"), *EndpointStr, *Message);

	if (!RpcHandler.IsValid())
	{
		return;
	}

	FString Response = RpcHandler->HandleRequest(Message);
	SendResponse(Response);
}

void UAClientConnection::SendResponse(const FString& Response)
{
	if (!Socket)
	{
		return;
	}

	UE_LOG(LogUnrealAgent, Verbose, TEXT("[%s] Sending: %s"), *EndpointStr, *Response);

	// Convert to UTF-8
	FTCHARToUTF8 Converter(*Response);
	int32 PayloadLength = Converter.Length();

	// Build Content-Length framed message
	FString Header = FString::Printf(TEXT("Content-Length: %d\r\n\r\n"), PayloadLength);
	FTCHARToUTF8 HeaderConverter(*Header);

	// Send header
	int32 BytesSent = 0;
	Socket->Send((const uint8*)HeaderConverter.Get(), HeaderConverter.Length(), BytesSent);

	// Send payload
	Socket->Send((const uint8*)Converter.Get(), PayloadLength, BytesSent);
}

void UAClientConnection::Close()
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}
