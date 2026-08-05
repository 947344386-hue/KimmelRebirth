// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Data/ClcMerchantConfig.h"
#include "ClcMerchantBubbleWidget.generated.h"

class USceneComponent;
class UClcMerchantOffScreenArrowWidget;

/**
 * 商人口头气泡——玩家进入话术范围时显示推销话术与单块声称。
 * 鹰眼心理话由独立的 UClcMerchantEagleEyeWidget 显示，可与本气泡同屏。
 */
UCLASS()
class CLAUDECORE_API UClcMerchantBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 设锚点——气泡中心跟随组件局部 Offset 转换后的屏幕投影。 */
	void SetAnchor(USceneComponent* InAnchorComponent, const FVector& InLocalOffset);

	/** 设置近大远小的屏幕空间模拟透视参数。 */
	void SetSimulatedPerspective(const FClcMerchantUISimulatedPerspectiveSettings& InSettings);

	/** 设置屏幕外指示器参数（开关 / 气泡到边缘留白 / 离屏固定缩放）。箭头位置由留白自动派生（气泡与边缘正中）。 */
	void SetOffScreenSettings(bool bEnabled, float EdgeMargin, float OffScale);

	/** 由商人 Actor Tick 调用；负责投影、模拟透视缩放、屏外钳制（边缘指示器）和重新入屏恢复 */
	void UpdateScreenPosition();

	/** 设气泡文字 */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetBubbleText(const FText& Text);

	/** 设次行文字（瞄准石头时的声称；传空则折叠次行不占位） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	void SetSecondaryText(const FText& Text);

protected:
	virtual void NativeDestruct() override;

	/** BP 绑定（可选，名字对上即用）——主行 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* BubbleTextBlock;

	/** BP 绑定（可选）——次行 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SecondaryTextBlock;

private:
	TWeakObjectPtr<USceneComponent> AnchorComponent;
	FVector AnchorLocalOffset = FVector::ZeroVector;
	FClcMerchantUISimulatedPerspectiveSettings SimulatedPerspective;

	/** 屏幕外指示器开关与参数（由商人配置传入；默认关，仅口头气泡开启，鹰眼洞察保持离屏隐藏）。 */
	bool bOffScreenEnabled = false;
	float OffScreenEdgeMargin = 60.f;
	float OffScreenScale = 1.0f;

	/** 独立屏幕空间离屏箭头 Widget（启用指示器时创建并加到视口；销毁时移除）。 */
	UPROPERTY(Transient)
	TObjectPtr<UClcMerchantOffScreenArrowWidget> ArrowWidget = nullptr;

	/** 背包打开时把气泡 Y 钳到此视口高度比例以下（避免被背包面板遮住）；<=0 关闭 */
	float BackpackOpenClampYFraction = 0.42f;
};
