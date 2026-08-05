// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tools/ClcStoneTool.h"
#include "ClcToolUpgradeMenuWidget.generated.h"

class UClcToolUpgradeEntryWidget;
class UButton;
class UTextBlock;
class UPanelWidget;

/** 一项商品的视图数据（配置 + 运行时状态） */
USTRUCT(BlueprintType)
struct FClcToolUpgradeItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ClcUpgrade")
	FClcToolUpgradeItem Item;

	UPROPERTY(BlueprintReadOnly, Category = "ClcUpgrade")
	bool bOwned = false;

	UPROPERTY(BlueprintReadOnly, Category = "ClcUpgrade")
	bool bAffordable = false;

	/** 在所属升级台 Upgrades 数组中的原始索引（用于过滤已拥有项后回查购买目标） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcUpgrade")
	int32 SourceIndex = INDEX_NONE;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradePurchaseRequested, int32, ItemIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeMenuClosed);

/**
 * 升级商店菜单（C++ 默认布局可用，BP 可选换皮）。
 * 列出商品、键盘/鼠标选中、确认购买。购买/关闭交由所属 AClcToolUpgradeStation 处理。
 */
UCLASS()
class CLAUDECORE_API UClcToolUpgradeMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcToolUpgradeMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** 填充商品列表（每次金币/所有权变化后由 Station 刷新） */
	void SetItems(const TArray<FClcToolUpgradeItemView>& InItems);

	/** 选中指定索引（由 Entry 点击或键盘调用） */
	void SelectItem(int32 Index);

	/** 确认购买当前选中项 → 广播 OnPurchaseRequested */
	UFUNCTION(BlueprintCallable, Category = "ClcUpgrade")
	void ConfirmSelected();

	/** 请求关闭菜单 → 广播 OnClosed */
	UFUNCTION(BlueprintCallable, Category = "ClcUpgrade")
	void RequestClose();

	/** 购买请求（Station 绑定） */
	UPROPERTY(BlueprintAssignable, Category = "ClcUpgrade|Events")
	FOnUpgradePurchaseRequested OnPurchaseRequested;

	/** 关闭请求（Station 绑定） */
	UPROPERTY(BlueprintAssignable, Category = "ClcUpgrade|Events")
	FOnUpgradeMenuClosed OnClosed;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UPanelWidget> ItemContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UButton> PurchaseButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UButton> CloseButton;

	/** 列表为空（全部升级已购买）时显示的提示文本 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> EmptyStateText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClcUpgrade|UI")
	TSubclassOf<UClcToolUpgradeEntryWidget> EntryWidgetClass;

private:
	UFUNCTION()
	void HandlePurchaseClicked();
	UFUNCTION()
	void HandleCloseClicked();

	void BuildDefaultLayout();
	void RefreshSelectionVisuals();
	void MoveSelection(int32 Delta);

	TArray<FClcToolUpgradeItemView> Items;
	TArray<TWeakObjectPtr<UClcToolUpgradeEntryWidget>> EntryWidgets;
	int32 SelectedIndex = INDEX_NONE;
};
