// Copyright KuoYu. All Rights Reserved.

#include "Settings/UASettings.h"

UUASettings::UUASettings()
	: ServerPort(55557)
	, bAutoStart(true)
	, BindAddress(TEXT("127.0.0.1"))
	, MaxConnections(16)
	, bVerboseLogging(false)
{
}
