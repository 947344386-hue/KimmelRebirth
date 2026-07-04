// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStoneVendor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "UI/ClcBackpackWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

AClcStoneVendor::AClcStoneVendor()
{
	PrimaryActorTick.bCanEverTick = false;

	VendorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VendorMesh"));
	RootComponent = VendorMesh;
	VendorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VendorMesh->SetGenerateOverlapEvents(false);

	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
}

void AClcStoneVendor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionIndicator)
	{
		InteractionIndicator->InteractionRadius = InteractionRadius;
	}
}

void AClcStoneVendor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Actor 销毁前清理——解绑委托 + 关背包，避免悬空委托
	if (bInSellMode)
	{
		ExitSellMode();
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================
// IClcInteractable
// ============================================================

FText AClcStoneVendor::GetInteractionPrompt() const
{
	return PromptText;
}

bool AClcStoneVendor::OnInteract(AActor* Interactor)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->GetLocalPlayer()) return false;

	UClcBackpackSubsystem* Backpack = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>();
	if (!Backpack) return false;

	// 已在出售模式 → 退出
	if (bInSellMode)
	{
		ExitSellMode();
		return true;
	}

	// 背包空 → 提示
	if (Backpack->GetStones().Num() == 0)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("背包空，无可出售的石头"));
		}
		return false;
	}

	EnterSellMode(Backpack);
	return true;
}

// ============================================================
// 出售模式
// ============================================================

void AClcStoneVendor::EnterSellMode(UClcBackpackSubsystem* Backpack)
{
	CachedBackpack = Backpack;
	bInSellMode = true;

	// 打开背包（ToggleBackpack 同步创建 Widget）
	if (!Backpack->IsBackpackOpen())
	{
		Backpack->ToggleBackpack();
	}

	// 绑定选石事件——玩家点石头时触发出售
	if (UClcBackpackWidget* Widget = Backpack->GetBackpackWidget())
	{
		Widget->OnStoneSelected.AddDynamic(this, &AClcStoneVendor::OnStoneSelectedForSale);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcVendor] BackpackWidget is null! Check BackpackWidgetClass in BackpackSubsystem config."));
		bInSellMode = false;
		CachedBackpack = nullptr;
		return;
	}

	OnEnterSellMode();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("出售模式：点击背包里的石头即可出售"));
	}
}

void AClcStoneVendor::ExitSellMode()
{
	if (!bInSellMode) return;
	bInSellMode = false;

	if (CachedBackpack)
	{
		// 解绑委托
		if (UClcBackpackWidget* Widget = CachedBackpack->GetBackpackWidget())
		{
			Widget->OnStoneSelected.RemoveDynamic(this, &AClcStoneVendor::OnStoneSelectedForSale);
		}

		// 关闭背包
		if (CachedBackpack->IsBackpackOpen())
		{
			CachedBackpack->ToggleBackpack();
		}

		CachedBackpack = nullptr;
	}

	OnExitSellMode();
}

void AClcStoneVendor::OnStoneSelectedForSale(int32 StoneIndex)
{
	if (!CachedBackpack) return;

	// GetStones 返回副本——取数据后 RemoveStone 不影响已拷贝的 StoneData
	TArray<FClcStoneRuntimeData> Stones = CachedBackpack->GetStones();
	if (StoneIndex < 0 || StoneIndex >= Stones.Num()) return;

	FClcStoneRuntimeData StoneData = Stones[StoneIndex];

	// 算回收价
	UClcStoneMarketSubsystem* Market = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>() : nullptr;
	if (!Market)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcVendor] MarketSubsystem unavailable!"));
		return;
	}

	const int32 SalePrice = Market->CalculateSalePrice(StoneData);

	// 出售：移除石头 + 加金币
	CachedBackpack->RemoveStone(StoneIndex);
	CachedBackpack->AddGold(SalePrice);

	// 刷新背包显示
	if (UClcBackpackWidget* Widget = CachedBackpack->GetBackpackWidget())
	{
		Widget->RefreshDisplay(CachedBackpack->GetStones(), CachedBackpack->GetGold());
	}

	// 通知蓝图（音效/特效/动画）
	OnStoneSold(StoneData, SalePrice);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("出售 %s，获得 %d 金币"), *StoneData.DisplayName, SalePrice));
	}

	UE_LOG(LogTemp, Log, TEXT("[ClcVendor] Sold '%s' for %d gold (backpack now has %d stones)"),
		*StoneData.DisplayName, SalePrice, CachedBackpack->GetStones().Num());

	// 背包空了 → 自动退出出售模式
	if (CachedBackpack->GetStones().Num() == 0)
	{
		ExitSellMode();
	}
}

// ============================================================
// BlueprintNativeEvent 默认实现（蓝图可覆写）
// ============================================================

void AClcStoneVendor::OnStoneSold_Implementation(const FClcStoneRuntimeData& StoneData, int32 SalePrice)
{
	// 蓝图覆写此函数播放音效/特效
}

void AClcStoneVendor::OnEnterSellMode_Implementation()
{
	// 蓝图覆写此函数播放进入动画
}

void AClcStoneVendor::OnExitSellMode_Implementation()
{
	// 蓝图覆写此函数播放退出动画
}
