// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcOpeningStone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcOpeningMaskComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Data/ClcShellTextureConfig.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

AClcOpeningStone::AClcOpeningStone()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// ---- 石头 Mesh ----
	StoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
	StoneMesh->SetupAttachment(RootComponent);
	StoneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StoneMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StoneMesh->SetCastShadow(true);

	// ---- 遮罩组件 ----
	OpeningMaskComp = CreateDefaultSubobject<UClcOpeningMaskComponent>(TEXT("OpeningMaskComp"));
}

void AClcOpeningStone::BeginPlay()
{
	Super::BeginPlay();
}

// ============================================================
// 初始化
// ============================================================

bool AClcOpeningStone::Initialize(const FClcStoneRuntimeData& StoneData, const FString& MaterialAssetPath)
{
	CachedStoneData = StoneData;

	// ---- 1. 加载石头 Mesh ----
	UStaticMesh* Mesh = nullptr;
	if (StoneData.Internal.StoneMesh.IsValid())
	{
		Mesh = StoneData.Internal.StoneMesh.Get();
	}
	else
	{
		Mesh = StoneData.Internal.StoneMesh.LoadSynchronous();
	}

	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcOpeningStone] Failed to load stone mesh!"));
		return false;
	}

	StoneMesh->SetStaticMesh(Mesh);
	StoneMesh->SetRelativeScale3D(FVector(StoneData.Internal.MeshScale));
	StoneMesh->SetMobility(EComponentMobility::Movable);

	// ---- 2. 加载开窗材质 ----
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath);
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcOpeningStone] Failed to load material: %s"), *MaterialAssetPath);
		return false;
	}

	StoneMesh->SetMaterial(0, Material);

	// ---- 3. 创建动态材质实例 ----
	StoneMID = StoneMesh->CreateDynamicMaterialInstance(0, Material, TEXT("StoneMID"));
	if (!StoneMID)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcOpeningStone] Failed to create MID!"));
		return false;
	}

	// ---- 3b. 从皮壳配置表取贴图，注入开窗 MID 的皮壳分支 ----
	if (UClcShellTextureConfig* ShellCfg = LoadObject<UClcShellTextureConfig>(
		nullptr, TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig")))
	{
		ShellCfg->InjectTexturesIntoMID(StoneMID, CachedStoneData.Internal.ShellTypeIndex);
	}

	// ---- 4. 初始化遮罩——有存档恢复存档，无存档从头开始 ----
	OpeningMaskComp->InitializeFromStoneData(StoneData.Internal);
	if (StoneData.SavedMaskBuffer.Num() > 0)
	{
		OpeningMaskComp->RestoreMaskFromData(StoneData);
	}
	OpeningMaskComp->ApplyToMaterial(StoneMID);

	// ---- 5. 初始化累计统计 ----
	AccumulatedOpenedRatio = 0.0f;
	AccumulatedGreenRatio = 0.0f;
	AccumulatedBlackRatio = 0.0f;

	bInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[ClcOpeningStone] Initialized stone '%s'. Green:%.2f Black:%.2f Grade:%d"),
		*StoneData.DisplayName, StoneData.Internal.GreenRatio,
		StoneData.Internal.BlackRatio, (int32)StoneData.Internal.Grade);

	return true;
}

// ============================================================
// 旋转
// ============================================================

void AClcOpeningStone::AddRotationInput(float DeltaPitch, float DeltaYaw)
{
	if (!bInitialized) return;

	// 世界空间四元数旋转——绕固定世界轴，彻底避免万向锁
	const FQuat CurrentQuat = StoneMesh->GetComponentQuat();
	const FQuat PitchQuat(FVector::RightVector, FMath::DegreesToRadians(DeltaPitch));
	const FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw));
	StoneMesh->SetWorldRotation(YawQuat * PitchQuat * CurrentQuat);
}

// ============================================================
// 打磨（委托——由 AClcOpeningTool 调用）
// ============================================================

bool AClcOpeningStone::GrindAtUV(float U, float V)
{
	if (!bInitialized || !OpeningMaskComp) return false;

	FClcStoneOpeningResult Result = OpeningMaskComp->GrindAtUV(U, V);

	AccumulatedOpenedRatio += Result.AreaFraction;
	AccumulatedGreenRatio += Result.NewGreenFraction;
	AccumulatedBlackRatio += Result.NewBlackFraction;

	// 首次开到绿——种水暴露，通知 Workbench 立即刷新 HUD
	if (Result.bHitGreen && !bGradeRevealed)
	{
		bGradeRevealed = true;
		bHUDDirty = true;
	}

	return true;
}

// ============================================================
// 存档
// ============================================================

void AClcOpeningStone::GetOpeningProgress(float& OutOpenedRatio, float& OutOpenedGreenRatio, float& OutOpenedBlackRatio) const
{
	if (OpeningMaskComp)
	{
		OutOpenedRatio = OpeningMaskComp->GetOpenedRatio();
	}
	else
	{
		OutOpenedRatio = AccumulatedOpenedRatio;
	}

	OutOpenedGreenRatio = AccumulatedGreenRatio;
	OutOpenedBlackRatio = AccumulatedBlackRatio;
}

bool AClcOpeningStone::GetStoneData(FClcStoneRuntimeData& OutData) const
{
	if (!bInitialized) return false;

	OutData = CachedStoneData;

	float OpenedRatio, GreenRatio, BlackRatio;
	GetOpeningProgress(OpenedRatio, GreenRatio, BlackRatio);

	const float SurfaceArea = CachedStoneData.Internal.SurfaceArea;

	OutData.AccumulatedOpenedArea = OpenedRatio * SurfaceArea;
	OutData.OpenedGreenArea = GreenRatio * SurfaceArea;
	OutData.OpenedBlackArea = BlackRatio * SurfaceArea;

	OutData.LargestExposedGreenPatch = GreenRatio * SurfaceArea;

	// 保存遮罩像素数据
	OpeningMaskComp->SaveMaskToData(OutData);

	return true;
}
