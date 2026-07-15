// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "ClcMerchantBubbleWidget.generated.h"

/**
 * 商人气泡——鹰眼激活时显示于商人头顶，单句内心独白。
 *
 * 位置由 C++ NativeTick 驱动：每帧把商人头顶世界位置投影到屏幕，SetPositionInViewport。
 * 生命周期由 AClcMerchant 管（ShowBubble/HideBubble），鹰眼只管开关。
 */
UCLASS()
class CLAUDECORE_API UClcMerchantBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 设锚点——气泡每帧跟随此 Actor 的 (ActorLocation + WorldOffset) 屏幕投影 */
	void SetAnchor(AActor* InMerchant, const FVector& InWorldOffset);

	/** 设屏幕偏移（从 config 传入） */
	void SetScreenOffset(const FVector2D& Offset) { ScreenOffset = Offset; }

	/** 设气泡文字 */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetBubbleText(const FText& Text);

	/** 设次行文字（鹰眼模式下的性格 tag；传空则折叠次行不占位） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetSecondaryText(const FText& Text);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** BP 绑定（可选，名字对上即用）——主行（嘴上话术 / 心理话） */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* BubbleTextBlock;

	/** BP 绑定（可选）——次行（鹰眼模式下的性格 tag） */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SecondaryTextBlock;

private:
	TWeakObjectPtr<AActor> AnchorMerchant;
	FVector AnchorWorldOffset = FVector::ZeroVector;
	FVector2D ScreenOffset = FVector2D::ZeroVector;
};
