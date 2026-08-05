// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tools/ClcStoneTool.h"
#include "ClcToolUpgradeEntryWidget.generated.h"

class UClcToolUpgradeMenuWidget;
class UButton;
class UTextBlock;

/**
 * 升级商店列表的一行（C++ 默认布局可用，BP 可选换皮：同名 BindWidgetOptional 控件覆盖）。
 * 由 UClcToolUpgradeMenuWidget 构造并填充；点击调用菜单的 SelectItem。
 */
UCLASS()
class CLAUDECORE_API UClcToolUpgradeEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 填充一行内容：商品配置 + 已拥有/买得起状态 + 索引 + 所属菜单 */
	void SetupEntry(const FClcToolUpgradeItem& Item, bool bOwned, bool bAffordable,
		int32 InIndex, UClcToolUpgradeMenuWidget* InOwner);

	/** 高亮/取消高亮（选中态） */
	void SetSelected(bool bSelected);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UButton> RowButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> DescText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcUpgrade|UI")
	TObjectPtr<UTextBlock> StateText;

private:
	UFUNCTION()
	void HandleClicked();

	void BuildDefaultLayout();
	void ApplyVisualState();

	int32 EntryIndex = INDEX_NONE;
	bool bEntryOwned = false;
	bool bEntrySelected = false;
	TWeakObjectPtr<UClcToolUpgradeMenuWidget> OwnerMenu;
};
