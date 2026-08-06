// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "ClcLog.h"
#include "UI/ClcBackpackWidget.h"
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

	if (BackpackWidget && BackpackWidget->IsInViewport())
	{
		BackpackWidget->RemoveFromParent();
	}
	BackpackWidget = nullptr;
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
			BackpackWidget->AddToViewport(100);
			BackpackWidget->RefreshDisplay(Stones, Gold);
			bIsOpen = true;
			PC->bShowMouseCursor = true;
			UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, BackpackWidget);
		}
	}
	else
	{
		if (BackpackWidget)
		{
			BackpackWidget->RemoveFromParent();
		}
		bIsOpen = false;
		PC->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC);
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
}
