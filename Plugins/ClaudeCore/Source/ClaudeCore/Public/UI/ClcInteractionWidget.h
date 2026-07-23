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

	/** 交互文案——选中时由中心组件推送 GetInteractionPrompt()（"购买 老坑玻璃种 - 1200 金币"等）。
	 *  per-actor 指示器 Widget 不实现也无害（BIE 可选）。屏幕中心准星实现此事件显示文案。 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcInteraction")
	void SetPromptText(const FText& Prompt);
};
