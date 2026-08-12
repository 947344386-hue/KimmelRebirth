// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "ClcLog.h"
#include "UI/ClcBackpackWidget.h"
#include "UI/ClcBackpackHudWidget.h"
#include "Data/ClcStoneConfig.h"
#include "ClcDeveloperSettings.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"

void UClcBackpackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!BackpackWidgetClass) { BackpackWidgetClass = LoadClass<UClcBackpackWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_Backpack.WBP_Backpack_C")); }

	// 从 DataAsset 读取初始金币——路径走 DeveloperSettings
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (DS && !DS->StoneConfigPath.IsEmpty())
	{
		if (UClcStoneConfig* Config = LoadObject<UClcStoneConfig>(nullptr, *DS->StoneConfigPath))
		{
			Gold = Config->InitialGold;
		}
	}

	// 延迟到下一 tick 注册 B 提示——Initialize 阶段跨子系统 GetSubsystem 可能尚未就绪
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::DeferredRegisterBPrompt);
		// HUD 不在这里创建：只在 HandlePostLoadMap → RebuildHud 中创建，确保仅游戏关卡显示
	}
}

void UClcBackpackSubsystem::DeferredRegisterBPrompt()
{
	if (BackpackPromptHandle != 0)
	{
		return;
	}

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
		{
			BackpackPromptHandle = KP->RegisterKeyPrompt(
				EKeys::B,
				NSLOCTEXT("ClcBackpack", "BackpackPromptLabel", "打开背包"),
				FName("Backpack"), 0);
			UE_LOG(LogClaudeCore, Log, TEXT("[ClcBackpack] B prompt registered, handle=%d"), BackpackPromptHandle);
			return;
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::DeferredRegisterBPrompt);
	}
}

void UClcBackpackSubsystem::DeferredCreateHud()
{
	if (HudWidget)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(World) : nullptr;
	if (!PC)
	{
		// PC 尚未就绪——下一 tick 重试（有上限防死循环）
		if (++HudCreateAttempts < 60)
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::DeferredCreateHud);
		}
		else
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcBackpack] DeferredCreateHud: PlayerController unavailable after %d attempts; gold bar hidden."), HudCreateAttempts);
		}
		return;
	}

	// 金币条样式优先用 WBP_BackpackHud（若用户建了换皮蓝图）；否则退回 C++ 默认布局
	if (!HudWidgetClass)
	{
		HudWidgetClass = LoadClass<UClcBackpackHudWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_BackpackHud.WBP_BackpackHud_C"));
		if (!HudWidgetClass)
		{
			HudWidgetClass = UClcBackpackHudWidget::StaticClass();
		}
	}

	if (HudWidgetClass)
	{
		HudWidget = CreateWidget<UClcBackpackHudWidget>(PC, HudWidgetClass);
		if (HudWidget)
		{
			HudWidget->AddToViewport(50);
			RefreshHud();
			UE_LOG(LogClaudeCore, Log, TEXT("[ClcBackpack] Backpack HUD gold bar created (class=%s)."), *HudWidgetClass->GetName());
		}
	}
}

void UClcBackpackSubsystem::Deinitialize()
{
	if (BackpackPromptHandle != 0)
	{
		if (ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
			{
				KP->UnregisterKeyPrompt(BackpackPromptHandle);
			}
		}
		BackpackPromptHandle = 0;
	}

	// 停掉滑动链 + 清本对象所有 next-tick 定时器（滑动链 + 延迟建 HUD 链），避免销毁后回调
	CancelSlide();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RemoveFromParent();
	}
	BackpackWidget = nullptr;

	// 移除常驻金币条 HUD
	if (HudWidget && HudWidget->IsInViewport())
	{
		HudWidget->RemoveFromParent();
	}
	HudWidget = nullptr;

	Super::Deinitialize();
}

// ---- 背包/金币数据访问 ----

TArray<FClcStoneRuntimeData> UClcBackpackSubsystem::GetStones() const { return Stones; }

int32 UClcBackpackSubsystem::GetGold() const { return Gold; }

void UClcBackpackSubsystem::ToggleBackpack()
{
	if (bSliding) return;

	APlayerController* PC = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
	if (!PC) return;

	if (!bIsOpen)
	{
		if (!BackpackWidget)
		{
			if (!BackpackWidgetClass) return;
			BackpackWidget = CreateWidget<UClcBackpackWidget>(PC, BackpackWidgetClass);
			if (!BackpackWidget) return;
		}
		if (BackpackWidget)
		{
			// 常驻金币条与背包互斥——打开背包时隐藏金币条
			if (HudWidget) { HudWidget->SetVisibility(ESlateVisibility::Hidden); }

			// 仅在未入Viewport时添加（关闭下滑途中再按B反向打开时不重复Add）
			if (!BackpackWidget->IsInViewport())
			{
				BackpackWidget->AddToViewport(100);
			}
			BackpackWidget->RefreshDisplay(Stones, Gold);
			// 上滑淡入到就位（若正在下滑则反向，进度不重置避免跳变）
			StartOpenSlide();
			bIsOpen = true;
			PC->bShowMouseCursor = true;
			UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, BackpackWidget);
		}
	}
	else
	{
		// 关闭：立即恢复输入/金币条，背包下滑淡出，动画结束由 TickSlide 自动移除
		bIsOpen = false;
		PC->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);

		if (HudWidget)
		{
			RefreshHud();
			HudWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		if (BackpackWidget)
		{
			StartCloseSlide();
		}
	}
}

void UClcBackpackSubsystem::RefreshHud()
{
	if (HudWidget)
	{
		HudWidget->SetGold(Gold);
		HudWidget->SetStoneCount(Stones.Num(), MAX_STONE_SLOTS);
	}
}

// ---- 操作 ----

int32 UClcBackpackSubsystem::AddStone(const FClcStoneRuntimeData& StoneData)
{
	int32 NewIndex = Stones.Add(StoneData);
	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	NotifySaveManagerTransaction();

	return NewIndex;
}

bool UClcBackpackSubsystem::RemoveStone(int32 StoneIndex)
{
	if (!Stones.IsValidIndex(StoneIndex)) return false;
	Stones.RemoveAt(StoneIndex);
	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	NotifySaveManagerTransaction();
	return true;
}

void UClcBackpackSubsystem::AddGold(int32 Amount)
{
	Gold += Amount;
	TotalEarned += Amount;
	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	NotifySaveManagerGoldChanged();
	// 通知任务系统刷新绝对型目标（EarnGold/ReachGoldTotal）
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
		{
			QS->NotifyObjectiveProgress(EClcQuestObjectiveType::EarnGold, 0);
		}
	}
}

bool UClcBackpackSubsystem::SpendGold(int32 Amount)
{
	if (Amount < 0) return false;
	if (Gold < Amount) return false;
	Gold -= Amount;
	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	NotifySaveManagerGoldChanged();
	// 持有金币变动可能触发 ReachGoldTotal（下降也可能达成低阈值目标）
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
		{
			QS->NotifyObjectiveProgress(EClcQuestObjectiveType::ReachGoldTotal, 0);
		}
	}
	return true;
}

void UClcBackpackSubsystem::GMAddGold(int32 Amount)
{
	AddGold(Amount);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcBackpack] GMAddGold +%d (total=%d)"), Amount, Gold);
}

// ---- 存档序列化 ----

void UClcBackpackSubsystem::RestoreFromSaveData(const FClcSaveData& Data)
{
	Stones = Data.SavedStones;
	Gold = Data.SavedGold;
	TotalEarned = Data.SavedTotalEarned;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcBackpack] 从存档恢复——Gold=%d, Stones=%d"), Gold, Stones.Num());

	// 读档后立即刷新 UI（HUD 常驻 + 背包若打开），避免依赖隐式时序
	RefreshHud();
	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
}

void UClcBackpackSubsystem::SetHudVisible(bool bVisible)
{
	if (HudWidget)
	{
		HudWidget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UClcBackpackSubsystem::SetSessionConfig(const FClcSessionConfig& Config)
{
	Gold = Config.StartingGold;
	// 起始资金不是玩家赚取的收益；赚钱任务从 0 开始累计。
	TotalEarned = 0;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcBackpack] 会话配置已应用 —— StartingGold=%d, Difficulty=%d"), Config.StartingGold, static_cast<uint8>(Config.Difficulty));
	RefreshHud();
}

void UClcBackpackSubsystem::RebuildHud()
{
	// 清理旧 HUD（可能已由旧关卡 GC，只剩悬空指针）
	if (HudWidget)
	{
		if (HudWidget->IsInViewport())
		{
			HudWidget->RemoveFromParent();
		}
		HudWidget = nullptr;
	}
	HudCreateAttempts = 0;
	DeferredCreateHud();
}

void UClcBackpackSubsystem::NotifySaveManagerGoldChanged()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;
	if (UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>())
	{
		SM->NotifyGoldChanged(Gold);
	}
}

void UClcBackpackSubsystem::NotifySaveManagerTransaction()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;
	if (UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>())
	{
		SM->NotifyTransactionCompleted();
	}
}

// ---- Slide Animation (unchanged) ----

void UClcBackpackSubsystem::StartOpenSlide()
{
	if (!BackpackWidget) return;
	SlideTarget = 1.0f;
	if (!bSliding)
	{
		bSliding = true;
		bRemoveOnComplete = false;
		LastSlideRealTime = FPlatformTime::Seconds();
		ApplySlideVisual();
		TickSlide();
	}
}

void UClcBackpackSubsystem::StartCloseSlide()
{
	if (!BackpackWidget) return;
	SlideTarget = 0.0f;
	if (!bSliding)
	{
		bSliding = true;
		bRemoveOnComplete = true;
		LastSlideRealTime = FPlatformTime::Seconds();
		TickSlide();
	}
}

void UClcBackpackSubsystem::TickSlide()
{
	if (!bSliding || !BackpackWidget) { bSliding = false; return; }

	double Now = FPlatformTime::Seconds();
	float dt = static_cast<float>(Now - LastSlideRealTime);
	LastSlideRealTime = Now;

	if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
	float step = dt / SlideDuration;
	if (SlideTarget > SlideAlpha)
	{
		SlideAlpha = FMath::Min(SlideAlpha + step, SlideTarget);
	}
	else
	{
		SlideAlpha = FMath::Max(SlideAlpha - step, SlideTarget);
	}
	ApplySlideVisual();
	if (FMath::IsNearlyEqual(SlideAlpha, SlideTarget, 0.001f))
	{
		bSliding = false;
		SlideAlpha = SlideTarget;
		ApplySlideVisual();
		if (bRemoveOnComplete)
		{
			BackpackWidget->RemoveFromParent();
			BackpackWidget = nullptr;
			bRemoveOnComplete = false;
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::TickSlide);
	}
}

void UClcBackpackSubsystem::CancelSlide()
{
	bSliding = false;
	bRemoveOnComplete = false;
}

void UClcBackpackSubsystem::ApplySlideVisual()
{
	if (!BackpackWidget) return;
	FVector2D Translation(0, (1.0f - SlideAlpha) * SlideOffsetY);
	BackpackWidget->SetRenderTranslation(Translation);
	BackpackWidget->SetRenderOpacity(SlideAlpha);
}

void UClcBackpackSubsystem::ShowNotification(const FString& Message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, Message);
	}
}
