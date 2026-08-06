// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
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

	// 延迟到下一 tick 注册 B 提示——Initialize 阶段跨子系统 GetSubsystem 可能尚未就绪，
	// 直接注册会被静默跳过（其他运行时注册的提示正常，唯独 Initialize 注册的 B 不出现）。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::DeferredRegisterBPrompt);
		// 同样延迟到下一 tick 创建常驻金币条 HUD——Initialize 阶段 PlayerController 可能尚未就绪。
		World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::DeferredCreateHud);
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
	UE_LOG(LogClaudeCore, Warning, TEXT("[ClcBackpack] DeferredRegisterBPrompt: KeyPromptSubsystem unavailable; B prompt will not show."));
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

	// 重置滑动状态，跨世界复用本子系统时从干净态开始
	bSliding = false;
	bRemoveOnComplete = false;
	SlideAlpha = 0.0f;
	SlideTarget = 0.0f;

	Super::Deinitialize();
}

void UClcBackpackSubsystem::ToggleBackpack()
{
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

		if (BackpackWidget && BackpackWidget->IsInViewport())
		{
			// 下滑淡出（若正在上滑则反向），到 0 后 TickSlide 自动 RemoveFromParent
			StartCloseSlide();
		}
	}
}

void UClcBackpackSubsystem::StartOpenSlide()
{
	if (!BackpackWidget)
	{
		return;
	}

	SlideTarget = 1.0f;
	bRemoveOnComplete = false;

	if (!bSliding)
	{
		bSliding = true;
		UWorld* World = GetWorld();
		LastSlideRealTime = World ? World->GetRealTimeSeconds() : 0.0;
		// 立即按当前 alpha 设视觉（首次打开 alpha=0 即透明+下移），避免满状态闪一帧
		ApplySlideVisual();
		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::TickSlide);
		}
	}
}

void UClcBackpackSubsystem::StartCloseSlide()
{
	if (!BackpackWidget)
	{
		return;
	}

	SlideTarget = 0.0f;
	bRemoveOnComplete = true;

	if (!bSliding)
	{
		bSliding = true;
		UWorld* World = GetWorld();
		LastSlideRealTime = World ? World->GetRealTimeSeconds() : 0.0;
		if (World)
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::TickSlide);
		}
	}
}

void UClcBackpackSubsystem::CancelSlide()
{
	// 中断 next-tick 链：TickSlide 下一帧看到 bSliding=false 即提前返回、不再续接
	bSliding = false;
	bRemoveOnComplete = false;
}

void UClcBackpackSubsystem::TickSlide()
{
	if (!bSliding || !BackpackWidget)
	{
		bSliding = false;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		bSliding = false;
		return;
	}

	const double Now = World->GetRealTimeSeconds();
	float Dt = static_cast<float>(Now - LastSlideRealTime);
	LastSlideRealTime = Now;
	Dt = FMath::Clamp(Dt, 0.0f, 0.1f); // 防卡顿后大跳

	const float Step = Dt / FMath::Max(SlideDuration, KINDA_SMALL_NUMBER);
	if (SlideTarget > SlideAlpha)
	{
		SlideAlpha = FMath::Min(SlideAlpha + Step, SlideTarget);
	}
	else if (SlideTarget < SlideAlpha)
	{
		SlideAlpha = FMath::Max(SlideAlpha - Step, SlideTarget);
	}

	ApplySlideVisual();

	if (FMath::IsNearlyEqual(SlideAlpha, SlideTarget))
	{
		bSliding = false;
		const bool bShouldRemove = bRemoveOnComplete;
		bRemoveOnComplete = false;
		if (bShouldRemove && BackpackWidget && BackpackWidget->IsInViewport())
		{
			// 关闭滑动完成——移除背包（RemoveFromParent 会清 tooltip）
			BackpackWidget->RemoveFromParent();
		}
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(this, &UClcBackpackSubsystem::TickSlide);
}

void UClcBackpackSubsystem::ApplySlideVisual()
{
	if (!BackpackWidget)
	{
		return;
	}
	// alpha 0 -> Y=+offset(下移) + opacity 0；alpha 1 -> Y=0(就位) + opacity 1
	BackpackWidget->SetRenderTranslation(FVector2D(0.0f, (1.0f - SlideAlpha) * SlideOffsetY));
	BackpackWidget->SetRenderOpacity(SlideAlpha);
}

void UClcBackpackSubsystem::RefreshHud()
{
	if (HudWidget)
	{
		HudWidget->SetGold(Gold);
		HudWidget->SetStoneCount(Stones.Num(), MAX_STONE_SLOTS);
	}
}

// ---- IClcStoneCarrier ----

TArray<FClcStoneRuntimeData> UClcBackpackSubsystem::GetStones() const
{
	return Stones;
}

int32 UClcBackpackSubsystem::AddStone(const FClcStoneRuntimeData& StoneData)
{
	if (Stones.Num() >= MAX_STONE_SLOTS)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcBackpack] MAX_STONE_SLOTS (%d) exceeded!"), MAX_STONE_SLOTS);
		return -1;
	}

	const int32 NewIndex = Stones.Add(StoneData);
	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();

	return NewIndex;
}

bool UClcBackpackSubsystem::RemoveStone(int32 StoneIndex)
{
	if (!Stones.IsValidIndex(StoneIndex)) return false;

	Stones.RemoveAt(StoneIndex);

	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	return true;
}

int32 UClcBackpackSubsystem::GetGold() const
{
	return Gold;
}

void UClcBackpackSubsystem::AddGold(int32 Amount)
{
	Gold += Amount;
	TotalEarned += FMath::Max(0, Amount);

	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
}

bool UClcBackpackSubsystem::SpendGold(int32 Amount)
{
	if (Gold < Amount)
	{
		ShowNotification(FString::Printf(TEXT("金币不足！需要 %d，当前 %d"), Amount, Gold));
		return false;
	}

	Gold -= Amount;

	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
	return true;
}

void UClcBackpackSubsystem::ShowNotification(const FString& Message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.5f, FColor::Yellow, Message);
	}
}

void UClcBackpackSubsystem::GMAddGold(int32 Amount)
{
	Gold += Amount;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
			FString::Printf(TEXT("[GM] Added %d gold. Total: %d"), Amount, Gold));
	}
	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}
	RefreshHud();
}
