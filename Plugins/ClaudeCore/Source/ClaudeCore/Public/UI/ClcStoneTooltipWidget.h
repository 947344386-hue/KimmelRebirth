// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneTooltipWidget.generated.h"

class UWidget;
class UTextBlock;

/**
 * 背包悬浮 tips 弹窗基类——独立控件，悬浮 StoneEntry 时动态创建+定位+销毁。
 *
 * C++ 端通过 BindWidgetOptional 直接绑定 BP 里同名的 TextBlock 组件，
 * ShowTooltip 默认实现里直接 SetText + 设颜色——BP 端几乎不用写逻辑，
 * 只需在 WBP_StoneTooltip 里放 4 个 TextBlock，名字分别叫：
 *   NameText / OriginText / ShellOrGradeText / ValueText
 *
 * 如果想做更花哨的布局（图标、分隔线、富文本等），BP 可以 override ShowTooltip 事件。
 */
UCLASS(Abstract)
class CLAUDECORE_API UClcStoneTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 按 Info 填充 tips 文本字段——C++ 默认实现会 SetText 四个 TextBlock。
	 * BlueprintNativeEvent：BP 可以 override 做自定义渲染，不 override 就走 C++ 默认。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ClcTooltip")
	void ShowTooltip(const FClcStoneTooltipInfo& Info);

	/** 设置锚点 widget——Tick 里检查锚点是否还可见，不可见就自动 Hide（解决父 widget 关闭时残留） */
	void SetAnchor(UWidget* InAnchor) { AnchorWidget = InAnchor; }

	/** 在 AnchorWidget 处创建并显示一个 Tooltip 实例。 */
	UFUNCTION(BlueprintCallable, Category = "ClcTooltip", meta = (WorldContext = "WorldContextObject"))
	static UClcStoneTooltipWidget* ShowTooltipNextTo(
		UObject* WorldContextObject,
		UWidget* AnchorWidget,
		const FClcStoneTooltipInfo& Info,
		TSubclassOf<UClcStoneTooltipWidget> TooltipClass);

	/** 从 Viewport 移除自己。 */
	UFUNCTION(BlueprintCallable, Category = "ClcTooltip")
	void Hide();

	/** tip 相对鼠标的像素偏移——BP 子类 Class Defaults 里配，默认鼠标右下 15 像素 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcTooltip")
	FVector2D TooltipOffset = FVector2D(15.0f, 15.0f);

	// ---- BP 绑定的 TextBlock（在 WBP_StoneTooltip 里放同名组件即可自动绑定） ----

	/** 名称行 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcTooltip|Text")
	UTextBlock* NameText;

	/** 产地行 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcTooltip|Text")
	UTextBlock* OriginText;

	/** 皮壳/种水行——bOpenedToJade=true 显示种水档位，false 显示皮壳名 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcTooltip|Text")
	UTextBlock* ShellOrGradeText;

	/** 价值行——格式"价格：1000（500↑）"，涨绿跌红等白 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcTooltip|Text")
	UTextBlock* ValueText;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 锚点 widget 弱引用——Tick 检查有效性，父 widget 关闭时自动销毁 tooltip */
	TWeakObjectPtr<UWidget> AnchorWidget;
};
