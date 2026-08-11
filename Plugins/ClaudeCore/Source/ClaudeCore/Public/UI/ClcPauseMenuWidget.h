// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcPauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UClcPauseMenuSubsystem;

/**
 * 暂停菜单 Widget —— C++ 默认布局 + BP 可选换皮。
 *
 * 模式：参照 UClcTeleportMenuWidget 的 BuildDefaultLayout + BindWidgetOptional。
 * 如果 WidgetTree 为空（无 WBP），BuildDefaultLayout() 自动创建完整默认 UI。
 * 如果有 WBP（Content/JadeBetting/UI/WBP_PauseMenu），BindWidgetOptional 自动绑定。
 *
 * ZOrder：150（由 Subsystem AddToViewport(150) 控制）。
 *
 * BindWidgetOptional 控件名：
 *   TitleText, ResumeButton, SaveButton, LoadButton, MainMenuButton, QuitButton
 */
UCLASS()
class CLAUDECORE_API UClcPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcPauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	/** 初始化——由 Subsystem 在 OpenPauseMenu 时调用 */
	void SetOwningSubsystem(UClcPauseMenuSubsystem* InSubsystem);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ---- BindWidgetOptional 控件（蓝图可用同名控件覆盖） ----

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UButton> MainMenuButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "PauseMenu|UI")
	TObjectPtr<UButton> QuitButton;

private:
	// ---- 按钮回调 ----
	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleQuitClicked();

	// ---- 默认布局 ----
	void BuildDefaultLayout();
	void AddButtonLabel(UButton* Button, const FString& Label);

	TWeakObjectPtr<UClcPauseMenuSubsystem> MenuSubsystem;
};
