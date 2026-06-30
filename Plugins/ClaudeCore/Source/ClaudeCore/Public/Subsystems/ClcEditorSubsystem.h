// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ClcEditorSubsystem.generated.h"

/**
 * 编辑器子系统——在工具栏添加中/英文切换按钮
 */
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
