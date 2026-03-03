// Copyright KuoYu. All Rights Reserved.

#include "Commands/UACommandBase.h"

TSharedPtr<FJsonObject> UACommandBase::MakeToolSchema(
	const FString& Name,
	const FString& Description,
	TSharedPtr<FJsonObject> InputSchema) const
{
	TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("name"), Name);
	Schema->SetStringField(TEXT("description"), Description);

	if (InputSchema.IsValid())
	{
		Schema->SetObjectField(TEXT("inputSchema"), InputSchema);
	}
	else
	{
		// Default empty object schema
		TSharedPtr<FJsonObject> EmptySchema = MakeShared<FJsonObject>();
		EmptySchema->SetStringField(TEXT("type"), TEXT("object"));
		EmptySchema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
		Schema->SetObjectField(TEXT("inputSchema"), EmptySchema);
	}

	return Schema;
}
