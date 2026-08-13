// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Quest/ClcQuestTypes.h"
#include "ClcQuestEntryWidget.generated.h"

class UTextBlock;
class UCheckBox;
class UProgressBar;

/**
 * 任务追踪面板一行的视图数据（配置 + 运行时状态）。
 *
 * ProgressText 是规范显示串（增量/绝对数值型为 "cur/param"，布尔型为 "✓"/"✗"），
 * 由 UClcQuestSubsystem::GetQuestProgressText 生成。
 *
 * 注意：绝对数值型（EarnGold/ReachGoldTotal）的 CurrentProgress 是陈旧值——
 * 实时值只在 GetQuestProgressText 内部从 Backpack 读取（只吐字符串），
 * 行内进度条对这两类应改用 ProgressText 文本显示。
 * 布尔型（UnlockUpgrade/ToolDamaged）的 CurrentProgress/ObjectiveParam 无意义。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcQuestEntryView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	FName QuestID;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	FString ProgressText;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	int32 CurrentProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	int32 ObjectiveParam = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	EClcQuestObjectiveType ObjectiveType = EClcQuestObjectiveType::CutStones;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest")
	EClcQuestCategory Category = EClcQuestCategory::SideQuest;
};

/**
 * 任务追踪面板的一行（C++ 默认行布局可用，BP 可选换皮：同名 BindWidgetOptional 控件覆盖）。
 *
 * 由 UClcQuestTrackerWidget 构造并调 SetupEntry，之后不再被 C++ 触碰。
 * 行内预设四种控件，按任务目标类型自动显隐：
 *  - 增量型/绝对数值型：DescText + ProgressText，CompleteCheck 隐藏
 *  - 绝对布尔型：DescText + CompleteCheck（达成=勾选），ProgressText 隐藏
 *  - 进度条 ProgressBar：仅主线 + 增量型目标显示（CurrentProgress/ObjectiveParam），其余隐藏
 *
 * BP 行子类（WBP_QuestEntryRow）如需进度条等额外表现：
 * SetupEntry 先于 NativeConstruct 执行，故在 NativeConstruct（Event Construct）里
 * 读取本类 BlueprintReadOnly 缓存字段填充即可。
 */
UCLASS()
class CLAUDECORE_API UClcQuestEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 填充一行内容（含按类型显隐控件） */
	void SetupEntry(const FClcQuestEntryView& View);

protected:
	virtual void NativeOnInitialized() override;

	/** 描述文本——恒显示 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest|UI")
	TObjectPtr<UTextBlock> DescText;

	/** 进度文本（"2/5"）——增量型/绝对数值型显示，布尔型隐藏 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest|UI")
	TObjectPtr<UTextBlock> ProgressText;

	/** 完成勾选——仅绝对布尔型显示（达成=勾选） */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest|UI")
	TObjectPtr<UCheckBox> CompleteCheck;

	/** 进度条——仅主线 + 增量型目标显示 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcQuest|UI")
	TObjectPtr<UProgressBar> ProgressBar;

	// ---- 缓存字段（BlueprintReadOnly，供 BP 行在 NativeConstruct 里做进度条/样式） ----

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	FName CachedQuestID;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	FText CachedDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	FString CachedProgressText;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	int32 CachedCurrentProgress = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	int32 CachedObjectiveParam = 1;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	EClcQuestObjectiveType CachedObjectiveType = EClcQuestObjectiveType::CutStones;

	UPROPERTY(BlueprintReadOnly, Category = "ClcQuest|UI")
	EClcQuestCategory CachedCategory = EClcQuestCategory::SideQuest;

private:
	void BuildDefaultLayout();
	void ApplyWidgetVisibility();
};
