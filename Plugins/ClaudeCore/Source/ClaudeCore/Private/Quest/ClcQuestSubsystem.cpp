// Copyright ClaudeCore. All Rights Reserved.

#include "Quest/ClcQuestSubsystem.h"
#include "Quest/ClcQuestConfig.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Tools/ClcStoneTool.h"
#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "UI/ClcQuestTrackerWidget.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool IsAbsoluteObjectiveType(EClcQuestObjectiveType Type)
	{
		return Type == EClcQuestObjectiveType::EarnGold ||
			Type == EClcQuestObjectiveType::ReachGoldTotal ||
			Type == EClcQuestObjectiveType::UnlockUpgrade ||
			Type == EClcQuestObjectiveType::ToolDamaged;
	}
}

const FString UClcQuestSubsystem::QuestConfigAssetPath = TEXT("/Game/JadeBetting/Data/DA_QuestConfig.DA_QuestConfig");

void UClcQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadQuestConfig();
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] Initialize —— 任务定义 %d 条"), QuestDefs.Num());
}

void UClcQuestSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (TrackerWidget && TrackerWidget->IsInViewport())
	{
		TrackerWidget->RemoveFromParent();
	}
	TrackerWidget = nullptr;

	Super::Deinitialize();
}

// ---- 配置加载 ----

void UClcQuestSubsystem::LoadQuestConfig()
{
	UClcQuestConfig* Config = LoadObject<UClcQuestConfig>(nullptr, *QuestConfigAssetPath);
	if (!Config)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] DA_QuestConfig 未找到（路径=%s），任务系统空跑"), *QuestConfigAssetPath);
		return;
	}

	QuestDefs.Empty();
	for (const FClcQuestData& Q : Config->Quests)
	{
		if (Q.QuestID.IsNone())
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] 跳过 QuestID 为空的任务定义"));
			continue;
		}
		if (QuestDefs.Contains(Q.QuestID))
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] QuestID 重复：%s，后者覆盖前者"), *Q.QuestID.ToString());
		}
		QuestDefs.Add(Q.QuestID, Q);
	}

	for (const auto& Pair : QuestDefs)
	{
		const FClcQuestData& Def = Pair.Value;
		if (!Def.NextQuestID.IsNone() && !QuestDefs.Contains(Def.NextQuestID))
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] 任务 %s 的 NextQuestID=%s 不存在"),
				*Pair.Key.ToString(), *Def.NextQuestID.ToString());
		}
		if (Def.ObjectiveType == EClcQuestObjectiveType::UnlockUpgrade)
		{
			const UEnum* UpgradeEnum = StaticEnum<EClcToolUpgrade>();
			if (!UpgradeEnum || !UpgradeEnum->IsValidEnumValue(Def.ObjectiveParam))
			{
				UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] 任务 %s 的解锁升级参数无效：%d"),
					*Pair.Key.ToString(), Def.ObjectiveParam);
			}
		}
	}

	// 初始化所有任务为 Inactive
	RuntimeStates.Empty();
	for (const auto& Pair : QuestDefs)
	{
		FClcQuestRuntimeState State;
		State.State = EClcQuestState::Inactive;
		State.CurrentProgress = 0;
		RuntimeStates.Add(Pair.Key, State);
	}
}

// ---- 任务查询 ----

const FClcQuestData* UClcQuestSubsystem::FindQuestDef(FName QuestID) const
{
	const FClcQuestData* Def = QuestDefs.Find(QuestID);
	return Def;
}

const FClcQuestRuntimeState* UClcQuestSubsystem::FindRuntimeState(FName QuestID) const
{
	return RuntimeStates.Find(QuestID);
}

FString UClcQuestSubsystem::GetQuestProgressText(FName QuestID) const
{
	const FClcQuestData* Def = QuestDefs.Find(QuestID);
	const FClcQuestRuntimeState* State = RuntimeStates.Find(QuestID);
	if (!Def || !State) return TEXT("0/0");

	int32 Cur = State->CurrentProgress;
	if (Def->ObjectiveType == EClcQuestObjectiveType::EarnGold)
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				Cur = BP->GetTotalEarned();
			}
		}
	}
	else if (Def->ObjectiveType == EClcQuestObjectiveType::ReachGoldTotal)
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				Cur = BP->GetGoldValue();
			}
		}
	}
	else if (Def->ObjectiveType == EClcQuestObjectiveType::UnlockUpgrade ||
		Def->ObjectiveType == EClcQuestObjectiveType::ToolDamaged)
	{
		// 布尔型：达成显示"是"，未达成显示"否"
		return CheckQuestComplete(QuestID) ? TEXT("是") : TEXT("否");
	}
	return FString::Printf(TEXT("%d/%d"), Cur, Def->ObjectiveParam);
}

// ---- 任务流转 ----

void UClcQuestSubsystem::AcceptQuest(FName QuestID)
{
	FClcQuestRuntimeState* State = RuntimeStates.Find(QuestID);
	if (!State)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] AcceptQuest: 未找到任务 %s"), *QuestID.ToString());
		return;
	}
	if (State->State != EClcQuestState::Inactive)
	{
		return; // 已接/已完成/已领取，忽略
	}

	State->State = EClcQuestState::Active;
	State->CurrentProgress = 0;
	if (const FClcQuestData* Def = QuestDefs.Find(QuestID))
	{
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] 接取任务 %s（类型=%d, 参数=%d）"),
			*QuestID.ToString(), static_cast<uint8>(Def->ObjectiveType), Def->ObjectiveParam);
	}
	else
	{
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] 接取任务 %s"), *QuestID.ToString());
	}

	// 接取后立即刷新绝对型条件；已拥有升级/已有耐久损失等状态可以立刻完成。
	if (const FClcQuestData* Def = QuestDefs.Find(QuestID); Def && IsAbsoluteObjectiveType(Def->ObjectiveType))
	{
		RefreshAbsoluteObjectives();
	}
	if (!bSuppressTrackerRefresh)
	{
		RefreshTracker();
	}
}

void UClcQuestSubsystem::AcceptAllQuests()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] AcceptAllQuests —— 自动接取链首任务"));

	// 收集所有 NextQuestID 的引用集合
	TSet<FName> ReferencedIDs;
	ReferencedIDs.Reserve(QuestDefs.Num());
	for (const auto& Pair : QuestDefs)
	{
		if (!Pair.Value.NextQuestID.IsNone())
		{
			ReferencedIDs.Add(Pair.Value.NextQuestID);
		}
	}

	// 链首 = 未被任何任务 NextQuestID 引用的任务
	for (const auto& Pair : QuestDefs)
	{
		if (!ReferencedIDs.Contains(Pair.Key))
		{
			AcceptQuest(Pair.Key);
		}
	}
}

void UClcQuestSubsystem::NotifyObjectiveProgress(EClcQuestObjectiveType Type, int32 Delta)
{
	if (IsAbsoluteObjectiveType(Type))
	{
		RefreshAbsoluteObjectives();
		return;
	}

	// 先快照事件发生前已经 Active 的匹配任务。
	// 完成后新接取的同类型任务不会吃到本次事件。
	TArray<FName> ActiveQuestIDs;
	for (const auto& Pair : QuestDefs)
	{
		if (Pair.Value.ObjectiveType != Type) continue;

		const FClcQuestRuntimeState* State = RuntimeStates.Find(Pair.Key);
		if (State && State->State == EClcQuestState::Active)
		{
			ActiveQuestIDs.Add(Pair.Key);
		}
	}

	bool bChanged = false;
	for (FName QuestID : ActiveQuestIDs)
	{
		FClcQuestRuntimeState* State = RuntimeStates.Find(QuestID);
		if (!State || State->State != EClcQuestState::Active) continue;

		State->CurrentProgress += Delta;
		bChanged = true;
		CompleteQuestAndChainNext(QuestID);
	}

	if (bChanged)
	{
		RefreshTracker();
	}
}

bool UClcQuestSubsystem::CheckQuestComplete(FName QuestID) const
{
	const FClcQuestData* Def = QuestDefs.Find(QuestID);
	if (!Def) return false;

	const FClcQuestRuntimeState* State = RuntimeStates.Find(QuestID);
	if (!State) return false;
	if (State->State != EClcQuestState::Active) return false;

	// 绝对型：从 Backpack 实时读
	if (Def->ObjectiveType == EClcQuestObjectiveType::EarnGold)
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				return BP->GetTotalEarned() >= Def->ObjectiveParam;
			}
		}
		return false;
	}
	if (Def->ObjectiveType == EClcQuestObjectiveType::ReachGoldTotal)
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				return BP->GetGoldValue() >= Def->ObjectiveParam;
			}
		}
		return false;
	}
	if (Def->ObjectiveType == EClcQuestObjectiveType::UnlockUpgrade)
	{
		const UEnum* UpgradeEnum = StaticEnum<EClcToolUpgrade>();
		if (!UpgradeEnum || !UpgradeEnum->IsValidEnumValue(Def->ObjectiveParam)) return false;

		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* TD = LP->GetSubsystem<UClcToolDurabilitySubsystem>())
			{
				return TD->OwnsUpgrade(static_cast<EClcToolUpgrade>(Def->ObjectiveParam));
			}
		}
		return false;
	}
	if (Def->ObjectiveType == EClcQuestObjectiveType::ToolDamaged)
	{
		// 布尔型：任意工具耐久 < Max 即达成（ObjectiveParam 不参与比较，仅作占位）
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (auto* TD = LP->GetSubsystem<UClcToolDurabilitySubsystem>())
			{
				const UEnum* ToolEnum = StaticEnum<EClcRepairableTool>();
				for (int32 i = 0; i < ToolEnum->NumEnums() - 1; ++i)
				{
					EClcRepairableTool Tool = static_cast<EClcRepairableTool>(ToolEnum->GetValueByIndex(i));
					if (Tool == EClcRepairableTool::None) continue;
					if (TD->GetMaxDurability(Tool) > 0.0f &&
						TD->GetDurability(Tool) < TD->GetMaxDurability(Tool) - KINDA_SMALL_NUMBER)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	// 增量型：比较累计进度
	return State->CurrentProgress >= Def->ObjectiveParam;
}

void UClcQuestSubsystem::CompleteQuestAndChainNext(FName QuestID)
{
	FClcQuestRuntimeState* State = RuntimeStates.Find(QuestID);
	if (!State || State->State != EClcQuestState::Active) return;

	// 必须先确认目标确实达成，否则不完成（增量型 CurrentProgress 未达标时跳过）
	if (!CheckQuestComplete(QuestID)) return;

	// 纯追求任务无奖励发放——状态直接置 Claimed（跳过"已完成待领取"中间态，
	// 因为没有领取动作）。若后续要奖励，可在此插入 Completed 中间态 + ClaimReward。
	State->State = EClcQuestState::Claimed;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] 任务完成 %s"), *QuestID.ToString());

	// 自动接取下一条
	const FClcQuestData* Def = QuestDefs.Find(QuestID);
	if (Def && !Def->NextQuestID.IsNone())
	{
		AcceptQuest(Def->NextQuestID);
	}
}

void UClcQuestSubsystem::RefreshAbsoluteObjectives()
{
	if (bRefreshingAbsoluteObjectives) return; // 防递归重入
	TGuardValue<bool> Guard(bRefreshingAbsoluteObjectives, true);

	bool bChanged = false;
	for (const auto& Pair : QuestDefs)
	{
		const FClcQuestData& Def = Pair.Value;
		if (!IsAbsoluteObjectiveType(Def.ObjectiveType)) continue;

		FClcQuestRuntimeState* State = RuntimeStates.Find(Pair.Key);
		if (!State || State->State != EClcQuestState::Active) continue;

		if (CheckQuestComplete(Pair.Key))
		{
			CompleteQuestAndChainNext(Pair.Key);
			bChanged = true;
		}
	}
	if (bChanged) RefreshTracker();
}

// ---- 存档 ----

void UClcQuestSubsystem::SerializeForSave(TMap<FName, FClcQuestRuntimeState>& OutStates) const
{
	OutStates = RuntimeStates;
}

void UClcQuestSubsystem::RestoreFromSaveData(const FClcSaveData& Data, bool bIsNewGame)
{
	if (bIsNewGame)
	{
		// 新游戏：全部 Inactive，由 AcceptAllQuests 接取链首
		return;
	}

	// 读档：恢复运行时状态
	RuntimeStates.Empty();
	for (const auto& Pair : QuestDefs)
	{
		FClcQuestRuntimeState State;
		State.State = EClcQuestState::Inactive;
		State.CurrentProgress = 0;
		RuntimeStates.Add(Pair.Key, State);
	}

	for (const auto& SavedPair : Data.SavedQuestStates)
	{
		if (FClcQuestRuntimeState* State = RuntimeStates.Find(SavedPair.Key))
		{
			*State = SavedPair.Value;
		}
	}

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] 从存档恢复 %d 条任务状态"), Data.SavedQuestStates.Num());
}

// ---- HUD ----

void UClcQuestSubsystem::RebuildTracker()
{
	if (TrackerWidget)
	{
		if (TrackerWidget->IsInViewport())
		{
			TrackerWidget->RemoveFromParent();
		}
		TrackerWidget = nullptr;
	}
	TrackerCreateAttempts = 0;
	DeferredCreateTracker();
}

void UClcQuestSubsystem::DeferredCreateTracker()
{
	if (TrackerWidget) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
	if (!PC)
	{
		if (++TrackerCreateAttempts < 60)
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UClcQuestSubsystem::DeferredCreateTracker);
		}
		else
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcQuest] DeferredCreateTracker: PlayerController 未就绪，放弃"));
		}
		return;
	}

	// 优先 WBP 换皮，否则 C++ 默认布局
	TSubclassOf<UClcQuestTrackerWidget> WidgetClass = LoadClass<UClcQuestTrackerWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_QuestTracker.WBP_QuestTracker_C"));
	if (!WidgetClass)
	{
		WidgetClass = UClcQuestTrackerWidget::StaticClass();
	}

	if (WidgetClass)
	{
		TrackerWidget = CreateWidget<UClcQuestTrackerWidget>(PC, WidgetClass);
		if (TrackerWidget)
		{
			TrackerWidget->AddToViewport(40);
			RefreshTracker();
			UE_LOG(LogClaudeCore, Log, TEXT("[ClcQuest] 追踪面板已创建 (class=%s)"), *WidgetClass->GetName());
		}
	}
}

void UClcQuestSubsystem::RefreshTracker()
{
	if (TrackerWidget)
	{
		TrackerWidget->RefreshDisplay(this);
	}
}
