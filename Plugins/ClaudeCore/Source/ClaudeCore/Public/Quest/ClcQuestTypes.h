// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcQuestTypes.generated.h"

/**
 * 任务目标条件类型 —— 决定任务如何被监听与推进。
 *
 * 增量型：每次对应玩法动作触发 +Delta，累加 CurrentProgress >= ObjectiveParam 即完成。
 * 绝对型：不累加事件，每次刷新时直接从 BackpackSubsystem 读当前值比对 ObjectiveParam。
 * ToolDamaged：绝对布尔型，任意工具耐久 < Max 即达成，ObjectiveParam 不参与比较。
 * UnlockUpgrade：绝对布尔型，ObjectiveParam 存 EClcToolUpgrade 的 int32 值；拥有该升级即达成。
 */
UENUM(BlueprintType)
enum class EClcQuestObjectiveType : uint8
{
    CutStones      UMETA(DisplayName = "解石数量", ToolTip = "增量型。ObjectiveParam=需完成的解石次数。每次解石 +1，达成次数即完成。"),
    UseWorkbench   UMETA(DisplayName = "擦石次数", ToolTip = "增量型。ObjectiveParam=需完成的擦石次数。每次擦石会话结束 +1。"),
    BuyStones      UMETA(DisplayName = "购买原石", ToolTip = "增量型。ObjectiveParam=需购买的原石数量。每次购买 +1。"),
    SellStones     UMETA(DisplayName = "卖出石头", ToolTip = "增量型。ObjectiveParam=需卖出的石头次数。每次卖出 +1。"),
    RepairTool     UMETA(DisplayName = "修理次数", ToolTip = "增量型。ObjectiveParam=需修理的次数。每次成功修理 +1。"),
    UnlockUpgrade  UMETA(DisplayName = "解锁升级", ToolTip = "绝对布尔型。ObjectiveParam=EClcToolUpgrade 枚举值（手电擦石器=0，解石台=1）。玩家拥有指定升级即达成。"),
    EarnGold       UMETA(DisplayName = "累计赚金币", ToolTip = "绝对型。ObjectiveParam=需累计赚取的金币总额。实时读 Backpack 的 TotalEarned 比对。"),
    ReachGoldTotal UMETA(DisplayName = "金币达到", ToolTip = "绝对型。ObjectiveParam=需达到的持有金币值。实时读 Backpack 当前 Gold 比对。"),
    ToolDamaged    UMETA(DisplayName = "工具耐久损失", ToolTip = "绝对布尔型。任意工具耐久 < Max 即达成。ObjectiveParam 不参与比较，留 1 占位即可。"),
};

/** 任务类型 —— 主线/支线，驱动左侧面板分组显示。 */
UENUM(BlueprintType)
enum class EClcQuestCategory : uint8
{
    MainQuest  UMETA(DisplayName = "主线"),
    SideQuest  UMETA(DisplayName = "支线"),
};

/** 任务状态机：Inactive → Active → Completed → Claimed。 */
UENUM(BlueprintType)
enum class EClcQuestState : uint8
{
    Inactive   UMETA(DisplayName = "未接取"),
    Active     UMETA(DisplayName = "进行中"),
    Completed  UMETA(DisplayName = "已完成待领取"),
    Claimed    UMETA(DisplayName = "已领取"),
};

/**
 * 任务定义 —— 由 DA_QuestConfig 配置，7 个字段驱动整个任务系统。
 *
 * 1. QuestID          唯一标识，链式接取靠它引用
 * 2. bShowOnTracker   是否显示在左侧追踪面板（隐藏监听任务设 false）
 * 3. ObjectiveType    任务条件类型
 * 4. ObjectiveParam    条件参数（增量=目标次数，绝对=目标值，UnlockUpgrade=升级枚举值）
 * 5. NextQuestID      完成后自动接取的任务 ID（空=链终结）
 * 6. Category         主线/支线
 * 7. DisplayName      面板显示名（空则回退用 QuestID）
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcQuestData
{
    GENERATED_BODY()

    /** 1.任务id（唯一） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FName QuestID;

    /** 2.是否显示在追踪面板（隐藏监听任务设 false，用于隐式拉起下一条） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    bool bShowOnTracker = true;

    /** 3.任务条件类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    EClcQuestObjectiveType ObjectiveType = EClcQuestObjectiveType::CutStones;

    /** 4.条件参数（增量=目标次数；绝对=目标值；UnlockUpgrade=EClcToolUpgrade 的 int32 值；ToolDamaged=忽略） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0", ToolTip = "依 ObjectiveType 而定：增量型=目标次数；EarnGold/ReachGoldTotal=目标金币值；UnlockUpgrade=EClcToolUpgrade 枚举值(手电擦石器=0,解石台=1)；ToolDamaged=忽略(留1占位)。"))
    int32 ObjectiveParam = 1;

    /** 5.完成后自动接取的任务id（空=链终结） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FName NextQuestID;

    /** 6.任务类型 主线/支线 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    EClcQuestCategory Category = EClcQuestCategory::SideQuest;

    /** 7.面板显示名（留空则按 ObjectiveType+ObjectiveParam 自动生成） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
    FText DisplayName;

    /**
     * 获取面板显示名：DisplayName 非空用 DisplayName，否则按 ObjectiveType+ObjectiveParam 自动生成。
     * 自动生成规则：解石数量→"解石 N 块"、擦石次数→"擦石 N 次"、购买原石→"购买 N 块原石"、
     * 卖出石头→"卖出 N 次石头"、修理次数→"修理 N 次"、解锁升级→"解锁升级 #枚举值"、
     * 累计赚金币→"累计赚 N 金币"、金币达到→"金币达到 N"、工具耐久损失→"工具耐久损失"。
     */
    FText GetDisplayName() const
    {
        if (!DisplayName.IsEmpty())
        {
            return DisplayName;
        }
        switch (ObjectiveType)
        {
        case EClcQuestObjectiveType::CutStones:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_CutStones", "解石 {0} 块"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::UseWorkbench:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_UseWorkbench", "擦石 {0} 次"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::BuyStones:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_BuyStones", "购买 {0} 块原石"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::SellStones:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_SellStones", "卖出 {0} 次石头"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::RepairTool:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_RepairTool", "修理 {0} 次"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::UnlockUpgrade:
            if (ObjectiveParam == 0)
            {
                return NSLOCTEXT("ClcQuest", "Auto_UnlockCombinedTool", "解锁手电擦石器");
            }
            if (ObjectiveParam == 1)
            {
                return NSLOCTEXT("ClcQuest", "Auto_UnlockCuttingTable", "解锁解石台");
            }
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_UnlockUpgrade", "解锁升级 #{0}"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::EarnGold:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_EarnGold", "累计赚 {0} 金币"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::ReachGoldTotal:
            return FText::Format(NSLOCTEXT("ClcQuest", "Auto_ReachGoldTotal", "金币达到 {0}"), FText::AsNumber(ObjectiveParam));
        case EClcQuestObjectiveType::ToolDamaged:
            return NSLOCTEXT("ClcQuest", "Auto_ToolDamaged", "工具耐久损失");
        default:
            return FText::FromName(QuestID);
        }
    }
};

/**
 * 任务运行时状态 —— 序列化单元，存档用。
 * 一个任务一条，Key=QuestID。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcQuestRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    EClcQuestState State = EClcQuestState::Inactive;

    /** 当前进度（增量型累加；绝对型不使用，实时从对应子系统读取） */
    UPROPERTY(SaveGame)
    int32 CurrentProgress = 0;
};
