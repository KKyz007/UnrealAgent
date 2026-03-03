// Copyright KuoYu. All Rights Reserved.

#include "Commands/UAPythonCommands.h"
#include "IPythonScriptPlugin.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

DEFINE_LOG_CATEGORY_STATIC(LogUAPython, Log, All);

TArray<FString> UAPythonCommands::GetSupportedMethods() const
{
	return {
		TEXT("execute_python"),
		TEXT("reset_python_context"),
	};
}

TSharedPtr<FJsonObject> UAPythonCommands::GetToolSchema(const FString& MethodName) const
{
	if (MethodName == TEXT("execute_python"))
	{
		TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
		InputSchema->SetStringField(TEXT("type"), TEXT("object"));

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();

		// code parameter (required)
		TSharedPtr<FJsonObject> CodeProp = MakeShared<FJsonObject>();
		CodeProp->SetStringField(TEXT("type"), TEXT("string"));
		CodeProp->SetStringField(TEXT("description"),
			TEXT("Python code to execute in the UE Editor context. "
				 "Use 'import unreal' to access the Unreal API. "
				 "Use print() for output. Context is shared across calls (stateful)."));
		Properties->SetObjectField(TEXT("code"), CodeProp);

		InputSchema->SetObjectField(TEXT("properties"), Properties);

		TArray<TSharedPtr<FJsonValue>> Required;
		Required.Add(MakeShared<FJsonValueString>(TEXT("code")));
		InputSchema->SetArrayField(TEXT("required"), Required);

		return MakeToolSchema(
			TEXT("execute_python"),
			TEXT("Execute Python code in the Unreal Editor context. "
				 "Has access to the full 'unreal' module API. "
				 "Context is stateful — variables and imports persist across calls. "
				 "Use print() to produce output."),
			InputSchema
		);
	}

	if (MethodName == TEXT("reset_python_context"))
	{
		return MakeToolSchema(
			TEXT("reset_python_context"),
			TEXT("Reset the shared Python execution context, clearing all variables and imports.")
		);
	}

	return nullptr;
}

bool UAPythonCommands::Execute(
	const FString& MethodName,
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (MethodName == TEXT("execute_python"))
	{
		return ExecutePython(Params, OutResult, OutError);
	}
	if (MethodName == TEXT("reset_python_context"))
	{
		return ExecuteResetContext(Params, OutResult, OutError);
	}

	OutError = FString::Printf(TEXT("Unknown method: %s"), *MethodName);
	return false;
}

bool UAPythonCommands::IsPythonAvailable(FString& OutError) const
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		OutError = TEXT("PythonScriptPlugin is not loaded. Enable it in Edit > Plugins.");
		return false;
	}

	if (!PythonPlugin->IsPythonAvailable())
	{
		OutError = TEXT("Python is not available. Check PythonScriptPlugin configuration.");
		return false;
	}

	if (!PythonPlugin->IsPythonInitialized())
	{
		// Try to force-enable Python
		PythonPlugin->ForceEnablePythonAtRuntime();

		if (!PythonPlugin->IsPythonInitialized())
		{
			OutError = TEXT("Python is not initialized. PythonScriptPlugin may still be loading.");
			return false;
		}
	}

	return true;
}

bool UAPythonCommands::ExecutePython(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	// Validate Python availability
	if (!IsPythonAvailable(OutError))
	{
		return false;
	}

	// Extract code parameter
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
	{
		OutError = TEXT("Invalid params: 'code' is required and must be non-empty");
		return false;
	}

	UE_LOG(LogUAPython, Log, TEXT("Executing Python (%d chars)"), Code.Len());

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();

	// Build the execution command
	// Use ExecuteFile mode with Public scope for stateful execution
	FPythonCommandEx PythonCommand;
	PythonCommand.Command = Code;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
	PythonCommand.Flags = EPythonCommandFlags::Unattended;

	// Execute
	const bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	// Collect log output (stdout/stderr)
	FString Output;
	FString Errors;

	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (Entry.Type == EPythonLogOutputType::Error)
		{
			if (!Errors.IsEmpty()) Errors += TEXT("\n");
			Errors += Entry.Output;
		}
		else
		{
			if (!Output.IsEmpty()) Output += TEXT("\n");
			Output += Entry.Output;
		}
	}

	// Truncate if too long
	if (Output.Len() > MaxOutputLength)
	{
		Output = Output.Left(MaxOutputLength);
		Output += FString::Printf(TEXT("\n... [output truncated at %d chars]"), MaxOutputLength);
	}
	if (Errors.Len() > MaxOutputLength)
	{
		Errors = Errors.Left(MaxOutputLength);
		Errors += FString::Printf(TEXT("\n... [errors truncated at %d chars]"), MaxOutputLength);
	}

	// Build result
	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), bSuccess);
	OutResult->SetStringField(TEXT("output"), Output);

	if (!PythonCommand.CommandResult.IsEmpty())
	{
		OutResult->SetStringField(TEXT("result"), PythonCommand.CommandResult);
	}

	if (!Errors.IsEmpty())
	{
		OutResult->SetStringField(TEXT("error"), Errors);
	}

	if (bSuccess)
	{
		UE_LOG(LogUAPython, Log, TEXT("Python execution succeeded. Output: %d chars"),
			Output.Len());
	}
	else
	{
		UE_LOG(LogUAPython, Warning, TEXT("Python execution failed: %s"),
			*PythonCommand.CommandResult);
	}

	return true; // We always return true to send the result; success/failure is in the JSON
}

bool UAPythonCommands::ExecuteResetContext(
	const TSharedPtr<FJsonObject>& Params,
	TSharedPtr<FJsonObject>& OutResult,
	FString& OutError)
{
	if (!IsPythonAvailable(OutError))
	{
		return false;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();

	// Reset by executing a script that clears the global namespace
	// We keep __builtins__ intact so Python still works
	FPythonCommandEx PythonCommand;
	PythonCommand.Command = TEXT(
		"_ua_keep = {'__builtins__', '__name__', '__doc__', '__package__', '__loader__', '__spec__', '_ua_keep'}\n"
		"_ua_to_del = [k for k in list(globals().keys()) if k not in _ua_keep]\n"
		"for _ua_k in _ua_to_del:\n"
		"    try:\n"
		"        del globals()[_ua_k]\n"
		"    except KeyError:\n"
		"        pass\n"
		"del _ua_keep, _ua_to_del\n"
		"print('Python context reset')\n"
	);
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
	PythonCommand.Flags = EPythonCommandFlags::Unattended;

	const bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	OutResult = MakeShared<FJsonObject>();
	OutResult->SetBoolField(TEXT("success"), bSuccess);
	OutResult->SetStringField(TEXT("message"),
		bSuccess ? TEXT("Python context reset successfully") : TEXT("Failed to reset Python context"));

	UE_LOG(LogUAPython, Log, TEXT("Python context reset: %s"), bSuccess ? TEXT("OK") : TEXT("FAILED"));

	return true;
}
