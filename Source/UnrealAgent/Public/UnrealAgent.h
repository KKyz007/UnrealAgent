// Copyright KuoYu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealAgent, Log, All);

class UATcpServer;

class FUnrealAgentModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<UATcpServer> TcpServer;

	void RegisterMenuEntry();
	void GenerateAgentMenu(FMenuBuilder& MenuBuilder);
	void ToggleServer();
};
