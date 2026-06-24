// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ClcEditorSubsystem.generated.h"
UCLASS()
class CLAUDECORE_API UClcEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	void RegisterToolbarButton();
	static void OnToggleLanguage();
};