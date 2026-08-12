// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Quest/ClcQuestTypes.h"
#include "ClcQuestSubsystem.generated.h"

class UClcQuestTrackerWidget;
struct FClcSaveData;

/**
 * 任务追踪子系统 —— LocalPlayerSubsystem。
 *
 * 职责：
 *  - 从 DA_QuestConfig 加载全部任务定义
 *  - 维护每个任务的运行时状态（Active/Completed/Claimed）
 *  - 接收玩法动作的进度通知（NotifyObjectiveProgress），推进增量型目标
 *  - 绝对型目标（EarnGold/ReachGoldTotal/UnlockUpgrade/ToolDamaged）实时从对应子系统读取
 *  - 任务完成后按 NextQuestID 自动接取下一条，形成主/支线链
 *  - 跨关卡重建左侧追踪面板
 *
 * 生命周期：
 *  - Initialize：同步加载 DA_QuestConfig（轻量），不做关卡相关初始化
 *  - GameInstance::HandlePostLoadMap 进游戏关卡时调 RebuildTracker + AcceptAllQuests（新游戏）
 *  - 玩法 Actor 各接入点调 NotifyObjectiveProgress
 *
 * 存档：SaveManager 的 CollectSaveData/DistributeSaveData 调 SerializeForSave/RestoreFromSaveData。
 */
UCLASS()
class CLAUDECORE_API UClcQuestSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- 任务查询 ----

    /** 任务定义（只读访问，供 HUD 显示标题/目标） */
    const FClcQuestData* FindQuestDef(FName QuestID) const;

    /** 当前所有任务运行时状态（HUD 遍历用） */
    const TMap<FName, FClcQuestRuntimeState>& GetRuntimeStates() const { return RuntimeStates; }

    /** 获取某任务运行时状态 */
    const FClcQuestRuntimeState* FindRuntimeState(FName QuestID) const;

    /** 获取某任务的进度文本（如 "5/10"），绝对型从对应子系统实时读取 */
    FString GetQuestProgressText(FName QuestID) const;

    // ---- 任务流转 ----

    /** 接取任务：Inactive→Active，初始化进度。已非 Inactive 则忽略。 */
    void AcceptQuest(FName QuestID);

    /** 新游戏开局：自动接取所有"链首"任务（QuestID 未被任何任务的 NextQuestID 引用）。 */
    void AcceptAllQuests();

    /**
     * 通知目标进度推进 —— 各玩法 Actor 接入点调用。
     * 增量型：只推进事件发生前已经 Active 的匹配任务，避免链式接取的下一条吃到同一次事件。
     * 绝对型（EarnGold/ReachGoldTotal/UnlockUpgrade/ToolDamaged）：忽略 Delta，直接读取对应子系统状态。
     * 推进后检查完成，完成后自动按 NextQuestID 接取下一条。
     */
    void NotifyObjectiveProgress(EClcQuestObjectiveType Type, int32 Delta = 1);

    // ---- 存档序列化 ----

    /** 收集所有任务状态到存档（SaveManager 调用） */
    void SerializeForSave(TMap<FName, FClcQuestRuntimeState>& OutStates) const;

    /** 从存档恢复任务状态（SaveManager 调用）。bIsNewGame=true 时忽略，走 AcceptAllQuests。 */
    void RestoreFromSaveData(const FClcSaveData& Data, bool bIsNewGame);

    // ---- HUD ----

    /** 关卡切换后重建追踪面板（GameInstance::HandlePostLoadMap 调用） */
    void RebuildTracker();

    /** 刷新追踪面板显示（进度变化时调用） */
    void RefreshTracker();

    /** 显/隐追踪面板（工作台独占流程中隐藏，避免与流程 HUD 冲突） */
    void SetTrackerVisible(bool bVisible);

private:
    /** 加载 DA_QuestConfig 到 QuestDefs */
    void LoadQuestConfig();

    /** 检查指定任务是否已完成（增量型=进度达标；绝对型=从 Backpack 读值达标） */
    bool CheckQuestComplete(FName QuestID) const;

    /** 标记任务 Completed，并按 NextQuestID 自动接取下一条（递归链） */
    void CompleteQuestAndChainNext(FName QuestID);

    /** 绝对型目标刷新：读取 Backpack/ToolDurability 状态并完成已满足的任务 */
    void RefreshAbsoluteObjectives();

    // ---- 配置 ----

    /** DA_QuestConfig 的固定资产路径 */
    static const FString QuestConfigAssetPath;

    // ---- 运行时 ----

    /** 任务定义表（QuestID → 定义） */
    TMap<FName, FClcQuestData> QuestDefs;

    /** 运行时状态表（QuestID → 状态） */
    TMap<FName, FClcQuestRuntimeState> RuntimeStates;

    /** 正在批量刷新绝对条件，防止链式接取时递归重入 */
    bool bRefreshingAbsoluteObjectives = false;

    /** 批量任务状态变更期间抑制中间 UI 刷新，结算结束后统一刷新一次 */
    bool bSuppressTrackerRefresh = false;

    // ---- 追踪面板 ----

    UPROPERTY(Transient)
    TObjectPtr<UClcQuestTrackerWidget> TrackerWidget;

    /** 延迟创建面板的重试计数 */
    int32 TrackerCreateAttempts = 0;

    void DeferredCreateTracker();
};
