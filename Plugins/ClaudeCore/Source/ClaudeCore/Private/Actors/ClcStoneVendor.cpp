// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStoneVendor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "UI/ClcBackpackWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

namespace
{
	UClcLogToastSubsystem* GetLogToast(APlayerController* PC)
	{
		if (!PC) return nullptr;
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			return LP->GetSubsystem<UClcLogToastSubsystem>();
		}
		return nullptr;
	}
}

AClcStoneVendor::AClcStoneVendor()
{
	// 每帧 Tick——WasInputKeyJustPressed 的 flag 只保留一帧，TickInterval>0 会漏检按键
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

	// 无缩放根——避免 BP 缩放 VendorMesh 污染 TriggerSphere 半径
	VendorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VendorRoot"));
	RootComponent = VendorRoot;

	VendorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VendorMesh"));
	VendorMesh->SetupAttachment(VendorRoot);
	VendorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	VendorMesh->SetGenerateOverlapEvents(false);

	// 范围触发器——只响应 Pawn Overlap
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(VendorRoot);
	TriggerSphere->InitSphereRadius(InteractionRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
}

void AClcStoneVendor::BeginPlay()
{
	Super::BeginPlay();

	// 同步半径到 TriggerSphere 和 Indicator
	TriggerSphere->SetSphereRadius(InteractionRadius);

	if (InteractionIndicator)
	{
		InteractionIndicator->InteractionRadius = InteractionRadius;
		// 范围选中模式：不要求瞄准，委托决定是否可选
		InteractionIndicator->bSelectByProximity = true;
		// 绑定委托：背包有石头 → 选中态；空背包 → 仅范围内态
		InteractionIndicator->OnQueryCanSelect.BindDynamic(this, &AClcStoneVendor::QueryCanSelect);
	}

	// 绑 TriggerSphere overlap
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcStoneVendor::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcStoneVendor::OnTriggerEndOverlap);
}

void AClcStoneVendor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedPC.IsValid()) return;

	// 非出售模式：范围内按 F 进入
	if (!bInSellMode && PlayerInRange.IsValid())
	{
		if (CachedPC->WasInputKeyJustPressed(EnterKey))
		{
			OnInteract(PlayerInRange.Get());
		}
	}
	// 出售模式：按 Esc 退出
	else if (bInSellMode)
	{
		if (CachedPC->WasInputKeyJustPressed(ExitKey))
		{
			if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC.Get()))
			{
				LT->AddLog(TEXT("已退出出售模式"), 2.0f, FLinearColor::White);
			}
			ExitSellMode();
		}
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
	// 出售模式中 F 不响应（只能 Esc 或背包空退出）
	if (bInSellMode) return false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->GetLocalPlayer()) return false;

	UClcBackpackSubsystem* Backpack = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>();
	if (!Backpack) return false;

	// 背包空 → 提示
	if (Backpack->GetStones().Num() == 0)
	{
		if (UClcLogToastSubsystem* LT = GetLogToast(PC))
		{
			LT->AddLog(TEXT("背包空，无可出售的石头"), 2.0f, FLinearColor::Yellow);
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

	if (UClcLogToastSubsystem* LT = GetLogToast(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		LT->AddLog(TEXT("出售模式：点击背包里的石头即可出售"), 2.0f, FLinearColor(0.f, 1.f, 1.f));
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

	if (UClcLogToastSubsystem* LT = GetLogToast(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		LT->AddLog(FString::Printf(TEXT("出售 %s，获得 %d 金币"), *StoneData.DisplayName, SalePrice),
			2.0f, FLinearColor::Green);
	}

	// 背包空了 → 自动退出出售模式
	if (CachedBackpack->GetStones().Num() == 0)
	{
		if (UClcLogToastSubsystem* LT = GetLogToast(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			LT->AddLog(TEXT("背包已空，已退出出售模式"), 2.0f, FLinearColor::Yellow);
		}
		ExitSellMode();
	}
}

// ============================================================
// TriggerSphere overlap
// ============================================================

void AClcStoneVendor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(Other))
	{
		if (Pawn->IsLocallyControlled())
		{
			PlayerInRange = Pawn;
			CachedPC = Cast<APlayerController>(Pawn->GetController());
		}
	}
}

void AClcStoneVendor::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (PlayerInRange.Get() == Other)
	{
		PlayerInRange.Reset();
		// 出售模式期间保留 CachedPC（Esc退出用）；非出售模式清掉
		if (!bInSellMode)
		{
			CachedPC.Reset();
		}
		// 注意：离开范围不自动退出出售模式——用户要求只能 Esc 或背包空退出
	}
}

// ============================================================
// InteractionIndicator 委托
// ============================================================

bool AClcStoneVendor::QueryCanSelect()
{
	// 背包有石头 → 选中态；空背包 → 仅范围内态
	APlayerController* PC = CachedPC.IsValid()
		? CachedPC.Get()
		: UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->GetLocalPlayer()) return false;

	if (UClcBackpackSubsystem* BP = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>())
	{
		return BP->GetStones().Num() > 0;
	}
	return false;
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
