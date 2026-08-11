// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcSessionTypes.h"
#include "ClcMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class USlider;
class UComboBoxString;
class UListView;
class UPanelWidget;
class UClcMainMenuSubsystem;

/**
 * 主菜单 Widget —— C++ 默认布局 + BP 可选换皮。
 *
 * 模式：参照 UClcTeleportMenuWidget 的 BuildDefaultLayout + BindWidgetOptional。
 * 如果 WidgetTree 为空（无 WBP），BuildDefaultLayout() 自动创建完整默认 UI。
 * 如果有 WBP，BindWidgetOptional 按名字自动绑到 BP 摆的控件，覆盖 C++ 默认。
 *
 * ZOrder：0（全屏覆盖，由 Subsystem AddToViewport(0) 控制）。
 *
 * BindWidgetOptional 控件名：
 *   TitleText, StartButton, ContinueButton, QuitButton
 *   GoldSlider, GoldValueText, DifficultyComboBox
 *   SaveSlotList, DeleteSaveButton, NoSavesText
 */
UCLASS()
class CLAUDECORE_API UClcMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** 初始化菜单——由 Subsystem 在 ShowMainMenu 时调用 */
	void InitializeMenu(UClcMainMenuSubsystem* InSubsystem);

	/** 刷新存档列表 */
	void RefreshSaveSlots();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---- BindWidgetOptional 控件（蓝图可用同名控件覆盖） ----

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UButton> StartButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<USlider> GoldSlider;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UTextBlock> GoldValueText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UComboBoxString> DifficultyComboBox;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UPanelWidget> SaveSlotContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UButton> DeleteSaveButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "MainMenu|UI")
	TObjectPtr<UTextBlock> NoSavesText;

	/** 存档条目基类——动态条目复用通用 UserWidget */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	TSubclassOf<UUserWidget> SaveSlotEntryClass;

	// ---- 蓝图可覆写事件 ----

	/** 玩家点击"开始新游戏" */
	UFUNCTION(BlueprintImplementableEvent, Category = "MainMenu|Events")
	void OnStartGame();

	/** 设置变更（金滑条/难度变动时） */
	UFUNCTION(BlueprintImplementableEvent, Category = "MainMenu|Events")
	void OnSettingsChanged(int32 NewGold, const FString& NewDifficulty);

private:
	// ---- 按钮回调 ----
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleDeleteSaveClicked();

	// ---- 滑条回调 ----
	UFUNCTION()
	void HandleGoldSliderChanged(float Value);

	// ---- 下拉框回调 ----
	UFUNCTION()
	void HandleDifficultyChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	// ---- 默认布局 ----
	void BuildDefaultLayout();

	/** 从 UI 控件读取当前会话配置 */
	FClcSessionConfig BuildSessionConfig() const;

	/** 更新金文本（滑条拖动时） */
	void UpdateGoldText(int32 Gold);

	TWeakObjectPtr<UClcMainMenuSubsystem> MenuSubsystem;

	/** 金滑条范围 */
	static constexpr int32 MinGold = 1000;
	static constexpr int32 MaxGold = 500000;
	static constexpr int32 DefaultGold = 50000;
};
