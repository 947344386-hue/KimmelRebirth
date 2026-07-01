// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcStoneInfoWidget.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Data/ClcShellTextureConfig.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

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

void AClcStone::BeginPlay()
{
	Super::BeginPlay();

	if (!InfoCardClass) { InfoCardClass = LoadClass<UClcStoneInfoWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_StoneInfo.WBP_StoneInfo_C")); }
}

void AClcStone::Initialize(const FClcStoneInternalData& InData, UStaticMesh* InMesh, float InScale, const FString& InDisplayName)
{
	RuntimeData.Internal = InData;
	RuntimeData.DisplayName = InDisplayName;
	RuntimeData.BackpackIndex = -1;
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

	// 应用皮壳材质 + 从配置表注入贴图
	if (UMaterialInterface* ShellMat = LoadObject<UMaterialInterface>(nullptr, *ShellMaterialPath))
	{
		StoneMesh->SetMaterial(0, ShellMat);
		if (UMaterialInstanceDynamic* ShellMID = StoneMesh->CreateDynamicMaterialInstance(0, ShellMat, TEXT("ShellMID")))
		{
			if (UClcShellTextureConfig* ShellCfg = LoadObject<UClcShellTextureConfig>(
				nullptr, TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig")))
			{
				ShellCfg->InjectTexturesIntoMID(ShellMID, InData.ShellTypeIndex);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcStone] Shell material not found: %s"), *ShellMaterialPath);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[ClcStone] Initialized: %s, Grade=%d, SA=%.1f, Price=%d"),
		*RuntimeData.DisplayName, (int32)InData.Grade, InData.SurfaceArea, InData.PurchasePrice);
}

FName AClcStone::GetShellName() const
{
	const int32 Idx = RuntimeData.Internal.ShellTypeIndex;
	if (UClcShellTextureConfig* ShellCfg = Cast<UClcShellTextureConfig>(
		StaticLoadObject(UClcShellTextureConfig::StaticClass(), nullptr,
			TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig"))))
	{
		if (const FClcShellTextureEntry* Entry = ShellCfg->GetEntryByIndex(Idx))
		{
			return Entry->ShellName;
		}
	}
	return NAME_None;
}

void AClcStone::RecalculateSurfaceArea()
{
	if (!StoneMesh || !StoneMesh->GetStaticMesh())
	{
		RuntimeData.Internal.SurfaceArea = 1000.0f;
		return;
	}

	// 从Mesh Bounds估算表面积
	const FBoxSphereBounds Bounds = StoneMesh->GetStaticMesh()->GetBounds();
	const float Radius = Bounds.SphereRadius * GetActorScale3D().GetMax();
	// 球体表面积近似：4πr²，乘以0.8做修正（不规则石头比球体小一点）
	RuntimeData.Internal.SurfaceArea = 4.0f * PI * Radius * Radius * 0.8f;
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

	UClcBackpackSubsystem* Backpack = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>();
	if (!Backpack) return false;

	if (!Backpack->SpendGold(RuntimeData.Internal.PurchasePrice))
	{
		return false;
	}

	Backpack->AddStone(RuntimeData);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("购买成功！%s 已加入背包"), *RuntimeData.DisplayName));
	}

	HideInfoCard();
	RemoveFromStall();

	return true;
}

bool AClcStone::PurchaseStone(AActor* Buyer)
{
	return OnInteract(Buyer);
}

void AClcStone::ShowInfoCard()
{
	if (bInfoCardVisible || !InfoCardClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	InfoCardWidget = CreateWidget<UClcStoneInfoWidget>(PC, InfoCardClass);
	if (InfoCardWidget)
	{
		InfoCardWidget->AddToViewport(50);
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
	Destroy();
}
