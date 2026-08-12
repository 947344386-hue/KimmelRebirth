// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStone.h"
#include "ClcLog.h"
#include "Actors/ClcStoneStall.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcStoneInfoWidget.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "ClcDeveloperSettings.h"
#include "Materials/MaterialInterface.h"
#include "Data/ClcShellTextureConfig.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

AClcStone::AClcStone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	StoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
	RootComponent = StoneMesh;
	StoneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StoneMesh->SetGenerateOverlapEvents(false);

	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
	InteractionIndicator->InteractionRadius = 400.0f; // 默认值，蓝图中可调
}

void AClcStone::SetOwningStall(AClcStoneStall* Stall)
{
	OwningStall = Stall;
}

void AClcStone::BeginPlay()
{
	Super::BeginPlay();

	// InfoCardClass 兜底：BP_Stone 子类若未在 Details 显式指定，按约定路径加载 WBP_StoneInfo。
	// 不兜底会导致 ShowInfoCard 因 InfoCardClass=null 直接 return，瞄准石头无信息卡。
	if (!InfoCardClass)
	{
		InfoCardClass = LoadClass<UClcStoneInfoWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_StoneInfo.WBP_StoneInfo_C"));
	}
}

void AClcStone::Initialize(const FClcStoneInternalData& InData, UStaticMesh* InMesh, float InScale, const FString& InDisplayName)
{
	RuntimeData.Internal = InData;
	RuntimeData.DisplayName = InDisplayName;
	RuntimeData.AccumulatedOpenedArea = 0.0f;
	RuntimeData.OpenedGreenArea = 0.0f;
	RuntimeData.OpenedBlackArea = 0.0f;
	RuntimeData.LargestExposedGreenPatch = 0.0f;

	if (InMesh)
	{
		StoneMesh->SetStaticMesh(InMesh);
	}

	SetActorScale3D(FVector(InScale));

	RecalculateSurfaceArea();

	// 表面积已基于真实 Mesh 重算——覆盖 GenerateStoneInternal 用占位 SA=1000 算的初值
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UClcStoneMarketSubsystem* Market = GI ? GI->GetSubsystem<UClcStoneMarketSubsystem>() : nullptr)
	{
		RuntimeData.Internal.TheoreticalValue = Market->CalculateTheoreticalValue(RuntimeData.Internal);
		RuntimeData.Internal.PurchasePrice = Market->CalculatePurchasePrice(RuntimeData.Internal);
	}
	else
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcStone] MarketSubsystem unavailable, prices based on placeholder SA=1000!"));
	}

	// 应用皮壳材质 + 从配置表注入贴图
	if (UMaterialInterface* ShellMat = LoadObject<UMaterialInterface>(nullptr, *ShellMaterialPath))
	{
		StoneMesh->SetMaterial(0, ShellMat);
		if (UMaterialInstanceDynamic* ShellMID = StoneMesh->CreateDynamicMaterialInstance(0, ShellMat, TEXT("ShellMID")))
		{
			if (UClcShellTextureConfig* ShellCfg = LoadObject<UClcShellTextureConfig>(
				nullptr, *GetDefault<UClcDeveloperSettings>()->ShellTextureConfigPath))
			{
				ShellCfg->InjectTexturesIntoMID(ShellMID, InData.ShellTypeIndex);
			}
		}
	}
	else
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcStone] Shell material not found: %s"), *ShellMaterialPath);
	}
}

FName AClcStone::GetShellName() const
{
	return UClcShellTextureConfig::GetShellName(RuntimeData.Internal.ShellTypeIndex);
}

void AClcStone::RecalculateSurfaceArea()
{
	if (!StoneMesh || !StoneMesh->GetStaticMesh())
	{
		RuntimeData.Internal.SurfaceArea = 1000.0f;
		RuntimeData.Internal.WeightKg = 0;
		return;
	}

	// 从Mesh Bounds估算表面积与重量
	const FBoxSphereBounds Bounds = StoneMesh->GetStaticMesh()->GetBounds();
	const float Scale = GetActorScale3D().GetMax();
	const float Radius = Bounds.SphereRadius * Scale;
	// 球体表面积近似：4πr²，乘以0.8做修正（不规则石头比球体小一点）
	RuntimeData.Internal.SurfaceArea = 4.0f * PI * Radius * Radius * 0.8f;

	// 重量：包围盒椭球体积 × 翡翠密度（3.3 g/cm³ ≈ 0.0033 kg/cm³），四舍五入到整公斤，至少 1 kg
	constexpr float JadeDensityKgPerCm3 = 0.0033f;
	const FVector HalfExtents = Bounds.BoxExtent * Scale;
	const float VolumeCm3 = (4.0f / 3.0f) * PI * HalfExtents.X * HalfExtents.Y * HalfExtents.Z;
	RuntimeData.Internal.WeightKg = FMath::Max(1, FMath::RoundToInt(VolumeCm3 * JadeDensityKgPerCm3));
}

void AClcStone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RangeCheckTimer -= DeltaTime;
	if (RangeCheckTimer > 0.0f) return;
	RangeCheckTimer = 0.3f;

	// 检查玩家是否在范围内且摄像机瞄准 → 显示信息卡片
	const int32 InteractionState = InteractionIndicator->GetInteractionState();
	bCameraAiming = (InteractionState == 2);

	if (bCameraAiming && !bInfoCardVisible)
	{
		ShowInfoCard();
	}
	else if (!bCameraAiming && bInfoCardVisible)
	{
		HideInfoCard();
	}
}

// ---- IClcInteractable ----

FText AClcStone::GetInteractionPrompt() const
{
	return FText::FromString(FString::Printf(TEXT("购买 %s - %d 金币"),
		*RuntimeData.DisplayName, RuntimeData.Internal.PurchasePrice));
}

bool AClcStone::OnInteract(AActor* Interactor)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return false;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return false;

	UClcBackpackSubsystem* Backpack = LP->GetSubsystem<UClcBackpackSubsystem>();
	if (!Backpack) return false;

	// 背包已满 → 不扣钱、不入库，提示玩家
	if (Backpack->GetStones().Num() >= UClcBackpackSubsystem::MAX_STONE_SLOTS)
	{
		if (UClcLogToastSubsystem* LT = LP->GetSubsystem<UClcLogToastSubsystem>())
		{
			LT->AddLog(FString::Printf(TEXT("背包已满（%d/%d），无法购入"),
				Backpack->GetStones().Num(), UClcBackpackSubsystem::MAX_STONE_SLOTS),
				2.0f, FLinearColor::Red);
		}
		return false;
	}

	if (!Backpack->SpendGold(RuntimeData.Internal.PurchasePrice))
	{
		if (UClcLogToastSubsystem* LT = LP->GetSubsystem<UClcLogToastSubsystem>())
		{
			LT->AddLog(TEXT("金币不足"), 2.0f, FLinearColor::Red);
		}
		return false;
	}

	Backpack->AddStone(RuntimeData);

	// 通知任务系统：购买原石 +1
	if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
	{
		QS->NotifyObjectiveProgress(EClcQuestObjectiveType::BuyStones, 1);
	}

	if (UClcLogToastSubsystem* LT = LP->GetSubsystem<UClcLogToastSubsystem>())
	{
		LT->AddLog(FString::Printf(TEXT("购买成功！%s 已加入背包"), *RuntimeData.DisplayName), 2.0f, FLinearColor::Green);
	}

	HideInfoCard();
	RemoveFromStall();

	return true;
}

bool AClcStone::PurchaseStone(AActor* Buyer)
{
	return OnInteract(Buyer);
}

void AClcStone::ApplyInteractionConfig(float InInteractionRadius, float InAimSweepRadius)
{
	if (InteractionIndicator)
	{
		InteractionIndicator->InteractionRadius = InInteractionRadius;
		InteractionIndicator->AimSweepRadius = InAimSweepRadius;
	}
}

void AClcStone::ShowInfoCard()
{
	if (bInfoCardVisible || !InfoCardClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	InfoCardWidget = CreateWidget<UClcStoneInfoWidget>(PC, InfoCardClass);
	if (InfoCardWidget)
	{
		InfoCardWidget->SetAnchor(this, InteractionIndicator->WidgetOffset);
		InfoCardWidget->AddToViewport(60);
		InfoCardWidget->UpdateScreenPosition();
		InfoCardWidget->ShowInfo(RuntimeData);
		bInfoCardVisible = true;
	}
}

void AClcStone::HideInfoCard()
{
	if (InfoCardWidget)
	{
		InfoCardWidget->HideInfo();
		InfoCardWidget->RemoveFromParent();
		InfoCardWidget = nullptr;
	}
	bInfoCardVisible = false;
}

void AClcStone::RemoveFromStall()
{
	HideInfoCard();
	// 通知摊位移除 + 广播给商人（必须在 Destroy 前调，摊位要读石头数据算购买结果）
	if (OwningStall.IsValid())
	{
		OwningStall->NotifyStoneRemoved(this);
	}
	Destroy();
}
