// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quest/ClcQuestTypes.h"
#include "ClcQuestTrackerWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UClcQuestSubsystem;

/**
 * 左侧常驻任务追踪面板 —— 主/支线分组显示。
 *
 * 只显示 State==Active && bShowOnTracker 的任务。
 * 主线条顶部一组，支线条下方一组，各带标题。
 * 完成后自动从面板消失（状态转 Claimed）。
 *
 * 生命周期由 UClcQuestSubsystem 管理：RebuildTracker 创建并 AddToViewport(40)；
 * 进度变化时 RefreshDisplay 刷新。
 */
UCLASS()
class CLAUDECORE_API UClcQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcQuestTrackerWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;

	/** 刷新显示——由 QuestSubsystem 调用 */
	void RefreshDisplay(class UClcQuestSubsystem* Subsystem);

protected:
	/** 主线标题文本（空列表时隐藏） */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest")
	TObjectPtr<UTextBlock> MainTitleText;

	/** 主线任务列表容器 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest")
	TObjectPtr<UVerticalBox> MainQuestList;

	/** 支线标题文本（空列表时隐藏） */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest")
	TObjectPtr<UTextBlock> SideTitleText;

	/** 支线任务列表容器 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest")
	TObjectPtr<UVerticalBox> SideQuestList;

private:
	void BuildDefaultLayout();

	/** 给一个 VerticalBox 填充任务条目（清空后重建） */
	void PopulateList(UVerticalBox* List, EClcQuestCategory Category, class UClcQuestSubsystem* Subsystem);
};
