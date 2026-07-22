// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "ClcMerchantBubbleWidget.generated.h"

/**
 * 商人口头气泡——玩家进入话术范围时显示推销话术与单块声称。
 * 鹰眼心理话由独立的 UClcMerchantEagleEyeWidget 显示，可与本气泡同屏。
 */
UCLASS()
class CLAUDECORE_API UClcMerchantBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 设锚点——气泡跟随此 Actor 的 (ActorLocation + WorldOffset) 屏幕投影 */
	void SetAnchor(AActor* InMerchant, const FVector& InWorldOffset);

	/** 设屏幕偏移（从 config 传入） */
	void SetScreenOffset(const FVector2D& Offset) { ScreenOffset = Offset; }

	/** 由商人 Actor Tick 调用；负责投影、屏外隐藏和重新入屏恢复 */
	void UpdateScreenPosition();

	/** 设气泡文字 */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetBubbleText(const FText& Text);

	/** 设次行文字（瞄准石头时的声称；传空则折叠次行不占位） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetSecondaryText(const FText& Text);

protected:
	/** BP 绑定（可选，名字对上即用）——主行 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* BubbleTextBlock;

	/** BP 绑定（可选）——次行 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SecondaryTextBlock;

private:
	TWeakObjectPtr<AActor> AnchorMerchant;
	FVector AnchorWorldOffset = FVector::ZeroVector;
	FVector2D ScreenOffset = FVector2D::ZeroVector;
};
