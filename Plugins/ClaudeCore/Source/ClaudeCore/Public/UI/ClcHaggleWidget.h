// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcHaggleConfig.h"
#include "ClcHaggleWidget.generated.h"

class UClcHaggleComponent;
class UTextBlock;
class UButton;
class UProgressBar;
class UPanelWidget;

/**
 * 讨价还价 QTE Widget——C++ 默认布局，蓝图可整体替换或逐控件覆盖。
 *
 * 相位（由 UClcHaggleComponent 驱动）：
 *   Selection：显示参考价 / NPC 报价 / 各档提示（按 1~N 加价）/ 直接出手按钮
 *   Playing  ：显示 WASD 方向序列 + 每键倒计时条，当前键高亮
 *   Result   ：显示结果文案（成功/失败/取消），随后由组件完成售出
 *
 * BindWidgetOptional 控件（蓝图可提供同名控件覆盖 C++ 默认）：
 *   NpcLineText / ReferencePriceText / AcceptButton / TierContainer /
 *   SequenceContainer / KeyTimerBar / ResultText
 *
 * QTE 键编码：0=W(↑) 1=A(←) 2=S(↓) 3=D(→)
 */
UCLASS()
class CLAUDECORE_API UClcHaggleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcHaggleWidget(const FObjectInitializer& ObjectInitializer);

	void SetOwningComponent(UClcHaggleComponent* InComp) { OwningComponent = InComp; }

	/** 进入选择阶段：填档位提示、参考价、NPC 报价 */
	void SetupSelection(int32 InReferencePrice, const TArray<FClcHaggleTier>& Tiers, const UClcHaggleConfig* Config);

	/** 进入 QTE：构造方向序列，高亮首个 */
	void StartSequence(const TArray<uint8>& Sequence);

	/** Playing 每帧刷新：当前键高亮 + 计时条（TimerFraction ∈ [0,1]） */
	void UpdatePlaying(int32 CurrentIndex, float TimerFraction);

	/** 结果阶段：显示文案，隐藏序列/选择 */
	void ShowResult(const FText& Line, bool bSuccess);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- BindWidgetOptional（蓝图可提供同名控件覆盖 C++ 默认布局） ----

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UTextBlock> NpcLineText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UTextBlock> ReferencePriceText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UButton> AcceptButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UPanelWidget> TierContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UPanelWidget> SequenceContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UProgressBar> KeyTimerBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcHaggle")
	TObjectPtr<UTextBlock> ResultText;

private:
	UFUNCTION()
	void HandleAcceptClicked();

	void BuildDefaultLayout();

	/** 纯文本 helper：WidgetTree 可用则用，否则 NewObject（兼容 BP 换皮） */
	UTextBlock* MakeTextBlock(const FString& Name);

	void ShowSelectionWidgets(bool bShow);
	void ShowPlayingWidgets(bool bShow);

	/** 方向箭头（0=↑ 1=← 2=↓ 3=→） */
	static const TCHAR* GlyphFor(uint8 KeyIndex);

	UPROPERTY(Transient)
	TWeakObjectPtr<UClcHaggleComponent> OwningComponent;

	// 序列箭头控件——用于每帧高亮当前键
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SequenceArrows;
};
