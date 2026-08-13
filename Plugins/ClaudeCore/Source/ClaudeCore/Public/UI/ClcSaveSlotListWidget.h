// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcSaveSlotListWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UBorder;
class UClcSaveManagerSubsystem;

/** 玩家在槽位列表中选中一个槽位（Save 模式=写入完成，Load 模式=请求加载）；空串=关闭 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveSlotPicked, const FString&, SlotName);

/** 2 级 UI 模式 */
UENUM(BlueprintType)
enum class EClcSaveSlotListMode : uint8
{
	Save UMETA(DisplayName = "保存——选择写入槽位"),
	Load UMETA(DisplayName = "读取——选择加载槽位"),
	Delete UMETA(DisplayName = "删除——选择删除槽位"),
};

/**
 * 单个槽位行 Widget —— 分段文本 + 按钮，点击广播槽位名。
 *
 * 分段锚点（BindWidgetOptional，WBP 里各放一个 TextBlock 自由排版）：
 *   SlotTitleText（"存档 1"/"自动存档"/"存档 5（空）"）
 *   SlotTimeText（"08-13 14:52"，空槽为空串）
 *   SlotGoldText（"金币 83250"，空槽为空串）
 *   SlotPlayTimeText（"3.6h"，空槽为空串）
 * 空串的段会被 Collapsed——WBP 里不摆某个段也没问题。
 *
 * 换皮：创建 WBP_SaveSlotRow（父类本类），放 SlotButton（UButton）+ 上述任意文本控件。
 */
UCLASS()
class CLAUDECORE_API UClcSaveSlotRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcSaveSlotRowWidget(const FObjectInitializer& ObjectInitializer);

	/** 初始化行——由槽位列表创建后调用；各段为空串则隐藏对应控件；bEmpty=空槽位时折叠分割线 */
	void InitRow(const FString& InSlotName, const FString& InTitle,
		const FString& InTime, const FString& InGold, const FString& InPlayTime,
		bool bEnabled, bool bEmpty);

	UPROPERTY(BlueprintAssignable, Category = "ClcSave")
	FOnSaveSlotPicked OnPicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UButton> SlotButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> SlotTitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> SlotTimeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> SlotGoldText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> SlotPlayTimeText;

	/** 分割线（可选）——空槽位自动折叠 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UBorder> DividerlineBorder;

private:
	UFUNCTION()
	void HandleClicked();

	FString SlotName;
};

/**
 * 存档槽位列表 Widget —— 手动存档/读取的 2 级弹出 UI。
 *
 * C++ 默认布局：UBorder 半透明暗底 + 居中 VerticalBox。
 * 换皮：创建 WBP_SaveSlotList（父类本类），提供同名控件（BindWidgetOptional）：
 *   - TitleText（UTextBlock）——标题
 *   - SlotListBox（UVerticalBox）——槽位行容器
 *   - CloseButton（UButton）——返回按钮
 * 行样式：创建 WBP_SaveSlotRow（父类 UClcSaveSlotRowWidget），
 *   并在 WBP_SaveSlotList 的 Class Defaults 里把 RowWidgetClass 设为它。
 */
UCLASS()
class CLAUDECORE_API UClcSaveSlotListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcSaveSlotListWidget(const FObjectInitializer& ObjectInitializer);

	/** 初始化——打开方创建后立即调用；此时构建默认布局（SaveManager 就位后再建） */
	void InitSlotList(UClcSaveManagerSubsystem* InSaveManager, EClcSaveSlotListMode InMode);

	/** 槽位被选中（Save 模式写档完成后 / Load 模式玩家点击后）；空串=玩家点了返回 */
	UPROPERTY(BlueprintAssignable, Category = "ClcSave")
	FOnSaveSlotPicked OnSlotPicked;

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---- 外壳控件（WBP 可用同名控件整体换皮） ----

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> TitleText;

	/** 槽位行容器 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UVerticalBox> SlotListBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UButton> CloseButton;

	// ---- 删除确认区（Delete 模式；WBP 可选） ----

	/** 确认容器——Delete 模式点击槽位后显示 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UBorder> ConfirmBorder;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UTextBlock> ConfirmText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UButton> ConfirmYesButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcSave")
	TObjectPtr<UButton> ConfirmNoButton;

	/** 槽位行 Widget 类——WBP 换皮时设为 WBP_SaveSlotRow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClcSave")
	TSubclassOf<UClcSaveSlotRowWidget> RowWidgetClass;

private:
	/** 无 WBP 时构建默认外壳（根+标题+列表容器+返回按钮+确认区） */
	void BuildShell();

	/** 按模式填充槽位行（默认外壳/WBP 共用） */
	void PopulateRows();

	UFUNCTION()
	void HandleSlotRowPicked(const FString& SlotName);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleConfirmYesClicked();

	UFUNCTION()
	void HandleConfirmNoClicked();

	/** 执行删除并刷新列表 */
	void ExecuteDelete(const FString& SlotName);

	/** 打开方提供的 SaveManager（只读引用，不持有） */
	TWeakObjectPtr<UClcSaveManagerSubsystem> SaveManager;
	EClcSaveSlotListMode Mode = EClcSaveSlotListMode::Save;

	/** Delete 模式：等待确认的槽位 */
	FString PendingDeleteSlot;
};
