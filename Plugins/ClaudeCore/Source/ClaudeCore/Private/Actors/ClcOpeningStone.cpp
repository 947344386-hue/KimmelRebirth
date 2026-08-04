// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcOpeningStone.h"
#include "ClcLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcOpeningMaskComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Data/ClcShellTextureConfig.h"
#include "Data/ClcJadeTextureConfig.h"
#include "ClcDeveloperSettings.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

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
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcOpeningStone] Failed to load stone mesh!"));
		return false;
	}

	StoneMesh->SetStaticMesh(Mesh);
	StoneMesh->SetRelativeScale3D(FVector(StoneData.Internal.MeshScale));
	StoneMesh->SetMobility(EComponentMobility::Movable);

	// ---- 2. 加载开窗材质 ----
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath);
	if (!Material)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcOpeningStone] Failed to load material: %s"), *MaterialAssetPath);
		return false;
	}

	StoneMesh->SetMaterial(0, Material);

	// ---- 3. 创建动态材质实例 ----
	StoneMID = StoneMesh->CreateDynamicMaterialInstance(0, Material, TEXT("StoneMID"));
	if (!StoneMID)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcOpeningStone] Failed to create MID!"));
		return false;
	}

	// ---- 3b. 从皮壳配置表取贴图，注入开窗 MID 的皮壳分支 ----
	// 路径：实例 UPROPERTY 优先，空则走 DeveloperSettings 全局配置
	const FString& ShellPath = ShellTextureConfigPath.IsEmpty()
		? GetDefault<UClcDeveloperSettings>()->ShellTextureConfigPath
		: ShellTextureConfigPath;
	if (UClcShellTextureConfig* ShellCfg = LoadObject<UClcShellTextureConfig>(
		nullptr, *ShellPath))
	{
		ShellCfg->InjectTexturesIntoMID(StoneMID, CachedStoneData.Internal.ShellTypeIndex);
	}

	// ---- 3c. 从玉石纹理配置表取高保真 PBR 纹理，注入开窗 MID 的玉/杂分支 ----
	const FString& JadePath = JadeTextureConfigPath.IsEmpty()
		? GetDefault<UClcDeveloperSettings>()->JadeTextureConfigPath
		: JadeTextureConfigPath;
	if (UClcJadeTextureConfig* JadeCfg = LoadObject<UClcJadeTextureConfig>(
		nullptr, *JadePath))
	{
		JadeCfg->InjectIntoMID(StoneMID);
	}

	// ---- 4. 初始化遮罩——有存档恢复存档，无存档从头开始 ----
	OpeningMaskComp->InitializeFromStoneData(StoneData.Internal);
	if (StoneData.SavedMaskBuffer.Num() > 0)
	{
		OpeningMaskComp->RestoreMaskFromData(StoneData);
	}
	OpeningMaskComp->ApplyToMaterial(StoneMID);

	// ---- 5. 初始化累计统计 ----
	// 有存档时从恢复的遮罩重建玉/杂质/裂暴露比例，无存档时为 0
	AccumulatedOpenedRatio = 0.0f;
	AccumulatedGreenRatio = 0.0f;
	AccumulatedImpurityRatio = 0.0f;
	AccumulatedCrackRatio = 0.0f;
	AccumulatedBlackRatio = 0.0f;
	if (StoneData.SavedMaskBuffer.Num() > 0 && OpeningMaskComp)
	{
		AccumulatedGreenRatio = OpeningMaskComp->GetExposedGreenRatio();
		AccumulatedOpenedRatio = OpeningMaskComp->GetOpenedRatio();
		AccumulatedImpurityRatio = OpeningMaskComp->GetExposedImpurityRatio();
		AccumulatedCrackRatio = OpeningMaskComp->GetExposedCrackRatio();
		AccumulatedBlackRatio = AccumulatedImpurityRatio + AccumulatedCrackRatio;
		// 已开到过绿 → 种水已暴露，HUD 直接显示种水信息（避免重载后回退显示皮壳）
		if (AccumulatedGreenRatio > 0.0f)
		{
			bGradeRevealed = true;
			bHUDDirty = true;
		}
	}

	bInitialized = true;

	// 记录初始旋转（用于 R 键复位）
	InitialMeshRotation = StoneMesh->GetComponentQuat();

	return true;
}

// ============================================================
// 旋转
// ============================================================

void AClcOpeningStone::AddRotationInput(float DeltaPitch, float DeltaYaw, const FVector& CameraRight, const FVector& CameraUp)
{
	if (!bInitialized) return;

	// 相机相对旋转：W/S 绕相机 Y 轴（屏幕左右旋转），A/D 绕相机 X 轴（屏幕上下倾斜）
	// 这样无论石头当前朝向如何，按键效果始终与屏幕方向一致
	const FQuat CurrentQuat = StoneMesh->GetComponentQuat();
	const FQuat PitchQuat(CameraRight, FMath::DegreesToRadians(DeltaPitch));  // A/D → 绕相机右轴
	const FQuat YawQuat(CameraUp, FMath::DegreesToRadians(DeltaYaw));         // W/S → 绕相机上轴
	StoneMesh->SetWorldRotation(YawQuat * PitchQuat * CurrentQuat);
}

bool AClcOpeningStone::ResetRotation(float DeltaTime, float InterpSpeed)
{
	if (!bInitialized) return true;

	const FQuat CurrentQuat = StoneMesh->GetComponentQuat();
	const FQuat TargetQuat = FMath::QInterpTo(CurrentQuat, InitialMeshRotation, DeltaTime, InterpSpeed);
	StoneMesh->SetWorldRotation(TargetQuat);

	return CurrentQuat.Equals(InitialMeshRotation, 0.001f);
}

// ============================================================
// 打磨（委托——由 AClcOpeningTool 调用）
// ============================================================

bool AClcOpeningStone::GrindAtUV(float U, float V)
{
	if (!bInitialized || !OpeningMaskComp) return false;

	// 已讨价还价锁定 → 禁止再开窗（价格已定，不能再改石头暴露）
	if (CachedStoneData.bHaggleResolved)
	{
		return false;
	}

	FClcStoneOpeningResult Result = OpeningMaskComp->GrindAtUV(U, V);

	AccumulatedGreenRatio += Result.NewGreenFraction;
	AccumulatedImpurityRatio += Result.NewImpurityFraction;
	AccumulatedCrackRatio += Result.NewCrackFraction;
	AccumulatedBlackRatio += Result.NewBlackFraction;

	// 首次开到绿——种水暴露，通知 Workbench 立即刷新 HUD + 飘字强化爽点
	if (Result.bHitGreen && !bGradeRevealed)
	{
		bGradeRevealed = true;
		bHUDDirty = true;

		// 飘字"见绿"——赌石核心时刻
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				if (UClcLogToastSubsystem* LT = ClcGetLogToast(PC))
				{
					FString GradeName = TEXT("玉");
					if (const UEnum* E = StaticEnum<EClcJadeGrade>())
					{
						GradeName = E->GetDisplayNameTextByValue(static_cast<int32>(CachedStoneData.Internal.Grade)).ToString();
					}
					LT->AddLog(FString::Printf(TEXT("见绿！种水初现：%s"), *GradeName), 2.5f, FLinearColor::Green);
				}
			}
		}
	}

	return true;
}

// ============================================================
// 存档
// ============================================================

void AClcOpeningStone::GetOpeningProgress(float& OutOpenedRatio, float& OutOpenedGreenRatio, float& OutOpenedBlackRatio,
	float& OutOpenedImpurityRatio, float& OutOpenedCrackRatio) const
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
	OutOpenedImpurityRatio = AccumulatedImpurityRatio;
	OutOpenedCrackRatio = AccumulatedCrackRatio;
	OutOpenedBlackRatio = AccumulatedBlackRatio;
}

bool AClcOpeningStone::GetStoneData(FClcStoneRuntimeData& OutData) const
{
	if (!bInitialized) return false;

	OutData = CachedStoneData;

	float OpenedRatio, GreenRatio, BlackRatio, ImpurityRatio, CrackRatio;
	GetOpeningProgress(OpenedRatio, GreenRatio, BlackRatio, ImpurityRatio, CrackRatio);

	const float SurfaceArea = CachedStoneData.Internal.SurfaceArea;

	OutData.AccumulatedOpenedArea = OpenedRatio * SurfaceArea;
	OutData.OpenedGreenArea = GreenRatio * SurfaceArea;
	OutData.OpenedImpurityArea = ImpurityRatio * SurfaceArea;
	OutData.OpenedCrackArea = CrackRatio * SurfaceArea;
	OutData.OpenedBlackArea = BlackRatio * SurfaceArea;

	// 最大绿色连通域——BFS 算真实连通面积（GreenRatio×SA = OpenedGreenArea，无连通性信息）
	if (OpeningMaskComp)
	{
		const int32 LargestGreenPixels = OpeningMaskComp->ComputeLargestGreenConnectedComponent();
		const float TotalPixels = static_cast<float>(UClcOpeningMaskComponent::MaskResolution * UClcOpeningMaskComponent::MaskResolution);
		OutData.LargestExposedGreenPatch = TotalPixels > 0.0f
			? (static_cast<float>(LargestGreenPixels) / TotalPixels) * SurfaceArea
			: 0.0f;
	}
	else
	{
		OutData.LargestExposedGreenPatch = 0.0f;
	}

	// 保存遮罩像素数据
	if (OpeningMaskComp)
	{
		OpeningMaskComp->SaveMaskToData(OutData);
	}

	return true;
}

void AClcOpeningStone::MarkHaggleResolved(int32 LockedPrice)
{
	CachedStoneData.bHaggleResolved = true;
	CachedStoneData.HaggleLockedPrice = LockedPrice;

	// 名字加锁价标记（防重复追加）——让背包/tooltip/HUD/售出提示统一体现锁定概念
	static const FString Suffix = TEXT("【已锁价】");
	if (!CachedStoneData.DisplayName.EndsWith(*Suffix))
	{
		CachedStoneData.DisplayName += Suffix;
	}
}

bool AClcOpeningStone::IsHaggleResolved() const
{
	return CachedStoneData.bHaggleResolved;
}
