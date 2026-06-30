// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcBackpackSubsystem.h"
#include "UI/ClcBackpackWidget.h"
#include "Data/ClcStoneConfig.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"

void UClcBackpackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BackpackWidgetClass = LoadClass<UClcBackpackWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_Backpack.WBP_Backpack_C"));

	// 从 DataAsset 读取初始金币
	if (UClcStoneConfig* Config = LoadObject<UClcStoneConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StoneConfig")))
	{
		Gold = Config->InitialGold;
	}
}

void UClcBackpackSubsystem::Deinitialize()
{
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
		UE_LOG(LogTemp, Error, TEXT("[ClcBackpack] MAX_STONE_SLOTS (%d) exceeded!"), MAX_STONE_SLOTS);
		return -1;
	}

	const int32 NewIndex = Stones.Add(StoneData);
	Stones[NewIndex].BackpackIndex = NewIndex;

	if (BackpackWidget && bIsOpen)
	{
		BackpackWidget->RefreshDisplay(Stones, Gold);
	}

	UE_LOG(LogTemp, Log, TEXT("[ClcBackpack] Added stone '%s' at slot %d. Total: %d"),
		*StoneData.DisplayName, NewIndex, Stones.Num());
	return NewIndex;
}

bool UClcBackpackSubsystem::RemoveStone(int32 StoneIndex)
{
	if (!Stones.IsValidIndex(StoneIndex)) return false;

	Stones.RemoveAt(StoneIndex);
	for (int32 i = StoneIndex; i < Stones.Num(); ++i)
	{
		Stones[i].BackpackIndex = i;
	}

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

FClcStoneRuntimeData* UClcBackpackSubsystem::GetStoneMutable(int32 Index)
{
	if (Stones.IsValidIndex(Index))
	{
		return &Stones[Index];
	}
	return nullptr;
}

void UClcBackpackSubsystem::UpdateStoneData(int32 Index, const FClcStoneRuntimeData& UpdatedData)
{
	if (Stones.IsValidIndex(Index))
	{
		Stones[Index] = UpdatedData;
		Stones[Index].BackpackIndex = Index;
	}
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
