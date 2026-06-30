// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcInteractionWidget.generated.h"

/**
 * AAA风格交互指示器——外圈环+内圈点
 */
UCLASS()
class CLAUDECORE_API UClcInteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 隐藏：全部不显示 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcInteraction")
	void SetStateHidden();

	/** 在范围内：只显示外圈环 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcInteraction")
	void SetStateInRange();

	/** 选中：外圈+内点 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcInteraction")
	void SetStateSelected();
};
