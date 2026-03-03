// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UASettings.generated.h"

/**
 * UnrealAgent plugin settings.
 * Configurable via Edit -> Project Settings -> UnrealAgent
 */
UCLASS(config=UnrealAgent, defaultconfig, meta=(DisplayName="UnrealAgent"))
class UNREALAGENT_API UUASettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUASettings();

	/** TCP server listen port */
	UPROPERTY(config, EditAnywhere, Category="Server", meta=(ClampMin=1024, ClampMax=65535))
	int32 ServerPort;

	/** Auto-start server when editor launches */
	UPROPERTY(config, EditAnywhere, Category="Server")
	bool bAutoStart;

	/** Bind address (127.0.0.1 = local only, 0.0.0.0 = allow remote) */
	UPROPERTY(config, EditAnywhere, Category="Server")
	FString BindAddress;

	/** Maximum concurrent connections */
	UPROPERTY(config, EditAnywhere, Category="Server", meta=(ClampMin=1, ClampMax=64))
	int32 MaxConnections;

	/** Enable verbose logging */
	UPROPERTY(config, EditAnywhere, Category="Debug")
	bool bVerboseLogging;

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
	virtual FName GetSectionName() const override { return FName(TEXT("UnrealAgent")); }
};
