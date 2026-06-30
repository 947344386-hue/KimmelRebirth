// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClaudeCore.h"

#define LOCTEXT_NAMESPACE "FClaudeCoreModule"

void FClaudeCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FClaudeCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaudeCoreModule, ClaudeCore)
