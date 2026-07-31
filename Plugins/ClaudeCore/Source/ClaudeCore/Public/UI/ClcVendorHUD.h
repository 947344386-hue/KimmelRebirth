// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcVendorHUD.generated.h"

class AClcStoneVendor;
class UButton;
class UBorder;
class UCanvasPanelSlot;
class UTextBlock;

/**
 * Vendor HUD 数据包——回收台展示模式用。
 * 相比 WorkbenchHUD 去掉工具字段，突出回收价 / 盈亏 / 钱包。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcVendorHUDData
{
	GENERATED_BODY()

	// ── 石头信息 ──

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	FString Origin;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	FString ShellName;

	/** 是否已暴露出玉石（OpenedGreenArea>0 时为 true，回收台不开窗，上台即定） */
	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	bool bGradeRevealed = false;

	/** EClcJadeGrade 索引，BP 侧自行查表映射文字和颜色 */
	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	uint8 GradeValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Stone")
	int32 PurchasePrice = 0;

	// ── 开窗展示 ──

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Opening")
	float OpenedRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Opening")
	float SurfaceArea = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Opening")
	float GreenArea = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Opening")
	float BlackArea = 0.0f;

	// ── 回收价（主信息） ──

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Price")
	int32 SalePrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Price")
	int32 ValuationTrend = 0;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Price")
	int32 ProfitAmount = 0;

	// ── 钱包 ──

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Wallet")
	int32 GoldBalance = 0;

	// ── 交互 ──

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Interaction")
	bool bCanSell = false;

	/** 背包是否打开——HUD 自适应定位：开→上方，关→居中 */
	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Interaction")
	bool bBackpackOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|Interaction")
	FString OperationHints = TEXT("WASD 旋转 | R 复位 | 右键 放大\nB 换石 | Enter 售出 | Esc 退出");

	// ── NPC 台词 ──

	/** NPC 当前台词——vendor 在高价值事件点填入对应文案，定时推送后 HUD 刷新显示 */
	UPROPERTY(BlueprintReadOnly, Category = "VendorHUD|NPC")
	FString NpcLine;
};

/**
 * 回收台展示 HUD——C++ 提供默认布局，蓝图可整体替换或逐控件覆盖。
 *
 * 模式（参照 ClcTeleportMenuWidget / ClcKeyPromptWidget）：
 *   NativeOnInitialized → BuildDefaultLayout（WidgetTree->ConstructWidget<T> 全控件构造）
 *   NativeConstruct → AddDynamic 绑按钮（搭配 RemoveDynamic 幂等）
 *   NativeDestruct → 解绑 + 清弱引用
 *   BindWidgetOptional：蓝图可提供同名控件覆盖 C++ 默认布局中的任意控件
 *
 * 蓝图无需任何逻辑——只需在 UMG 里摆控件并按下列名字命名（自动绑定）：
 *   SalePriceText / ProfitText / DisplayNameText / OriginText / GradeText
 *   OpenedRatioText / GreenAreaText / BlackAreaText / GoldText / HintsText / SellButton
 */
UCLASS()
class CLAUDECORE_API UClcVendorHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcVendorHUD(const FObjectInitializer& ObjectInitializer);

	/** Vendor 定时调用，把数据推给各控件（C++ 实现，BP 无需覆写） */
	void RefreshData(const FClcVendorHUDData& Data);

	/** 售出按钮点击回调——NativeConstruct 自动绑定，也供蓝图等外部主动调用 */
	UFUNCTION(BlueprintCallable, Category = "VendorHUD")
	void RequestSellFromUI();

	/** 持有所有者 Vendor 弱引用（CreateWidget 后由 Vendor 设置） */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "VendorHUD")
	TWeakObjectPtr<AClcStoneVendor> OwningVendor;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- BindWidgetOptional 控件（蓝图可提供同名控件覆盖 C++ 默认） ----

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> SalePriceText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> ProfitText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> DisplayNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> OriginText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> GradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> OpenedRatioText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> GreenAreaText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> BlackAreaText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> HintsText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UButton> SellButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UTextBlock> NpcLineText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "VendorHUD")
	TObjectPtr<UBorder> NpcDialogBox;

private:
	UFUNCTION()
	void HandleSellClicked();

	void BuildDefaultLayout();
	void UpdateCardPositions(bool bBackpackOpen);

	// 卡片 Slots（BuildDefaultLayout 创建，RefreshData 按背包状态调位置）
	UCanvasPanelSlot* LeftCardSlot = nullptr;
	UCanvasPanelSlot* RightCardSlot = nullptr;
	UCanvasPanelSlot* HintsSlot = nullptr;
};