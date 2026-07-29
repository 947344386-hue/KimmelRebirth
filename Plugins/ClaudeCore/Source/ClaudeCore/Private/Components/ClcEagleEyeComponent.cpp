// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcEagleEyeComponent.h"
#include "ClcLog.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "ClcDeveloperSettings.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcMerchant.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UClcEagleEyeComponent::UClcEagleEyeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UClcEagleEyeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeConfig();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			MarketSubsystem = GI->GetSubsystem<UClcStoneMarketSubsystem>();
		}
	}
}

void UClcEagleEyeComponent::InitializeConfig()
{
	Config = LoadObject<UClcEagleEyeConfig>(nullptr, *GetDefault<UClcDeveloperSettings>()->EagleEyeConfigPath);
	if (!Config)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcEagleEye] Failed to load EagleEyeConfig! Path: %s (check Project Settings → ClaudeCore)"),
			*GetDefault<UClcDeveloperSettings>()->EagleEyeConfigPath);
	}
}

bool UClcEagleEyeComponent::InitializeScanEffect()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Config || !World || World->GetNetMode() == NM_DedicatedServer || !Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	if (ScanPostProcessComponent && ScanMID)
	{
		return true;
	}

	UMaterialInterface* ScanMaterial = Config->ScanPostProcessMaterial.LoadSynchronous();
	if (!ScanMaterial)
	{
		UE_LOG(LogClaudeCore, Warning,
			TEXT("[ClcEagleEye] Scan material is not configured. Material=%s"),
			*Config->ScanPostProcessMaterial.ToString());
		CleanupScanEffect();
		return false;
	}

	if (!ScanMID)
	{
		ScanMID = UMaterialInstanceDynamic::Create(ScanMaterial, this);
		if (!ScanMID)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcEagleEye] Failed to create scan dynamic material instance."));
			CleanupScanEffect();
			return false;
		}
	}

	if (!ScanPostProcessComponent)
	{
		ScanPostProcessComponent = NewObject<UPostProcessComponent>(Pawn, NAME_None, RF_Transient);
		if (!ScanPostProcessComponent)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcEagleEye] Failed to create scan post-process component."));
			CleanupScanEffect();
			return false;
		}

		ScanPostProcessComponent->bUnbound = true;
		ScanPostProcessComponent->bEnabled = true;
		ScanPostProcessComponent->BlendWeight = 1.0f;
		ScanPostProcessComponent->Priority = 100.0f;
		ScanPostProcessComponent->AddOrUpdateBlendable(ScanMID, 1.0f);
		ScanPostProcessComponent->RegisterComponent();
	}

	return true;
}

void UClcEagleEyeComponent::StartOrRestartScanPulse(const FVector& Center)
{
	if (!Config || Config->ScanRadius <= 0.0f)
	{
		CleanupScanEffect();
		return;
	}

	if (!InitializeScanEffect())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !ScanMID)
	{
		CleanupScanEffect();
		return;
	}

	// ScanFX 扫描波靠材质内部“当前游戏时间 - Scan Start Time”自动扩散，
	// 这里只需在按键瞬间写入起点与参数，材质自行按时间扩散并在 Scan Duration 后淡出。
	static const FName ScanStartLocationName(TEXT("Scan Start Location"));
	static const FName ScanStartTimeName(TEXT("Scan Start Time"));
	static const FName ScanDurationName(TEXT("Scan Duration"));
	static const FName ScanRadiusName(TEXT("Scan Radius"));

	const float Duration = FMath::Max(0.0f, Config->ScanDuration);
	const float Radius = FMath::Max(0.0f, Config->ScanRadius);

	ScanMID->SetVectorParameterValue(ScanStartLocationName, FLinearColor(Center.X, Center.Y, Center.Z, 1.0f));
	ScanMID->SetScalarParameterValue(ScanStartTimeName, World->GetTimeSeconds());
	ScanMID->SetScalarParameterValue(ScanDurationName, Duration);
	ScanMID->SetScalarParameterValue(ScanRadiusName, Radius);

	bScanActive = true;
	// 扩散时长 + 余量，让材质淡出完成后再销毁后处理组件
	ScanEndTimer = Duration + 1.0f;
}

bool UClcEagleEyeComponent::CanRunLocalXRay() const
{
	const UWorld* World = GetWorld();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Config && Config->bEnableXRayScan && World && World->GetNetMode() != NM_DedicatedServer
		&& Pawn && Pawn->IsLocallyControlled();
}

void UClcEagleEyeComponent::StartOrRestartXRayScan(const FVector& Center)
{
	if (!CanRunLocalXRay()) return;
	DestroyActiveXRayScan();

	UWorld* World = GetWorld();
	if (!World || !Config) return;

	const float Radius = FMath::Max(0.0f, Config->ScanRadius);
	const float RadiusSq = FMath::Square(Radius);

	// XRay 三角扫描材质——默认 ScanFX 的 MI_ScanFX_TriangleScanner，可在 DA_EagleEyeConfig 换成自定义 MI
	static const TCHAR* DefaultXRayMaterialPath =
		TEXT("/Game/ScanFX/Materials/Instances/MI_ScanFX_TriangleScanner.MI_ScanFX_TriangleScanner");
	UMaterialInterface* ScanMaterial = Config->XRayScanMaterial.IsNull()
		? nullptr : Config->XRayScanMaterial.LoadSynchronous();
	if (!ScanMaterial)
	{
		ScanMaterial = LoadObject<UMaterialInterface>(nullptr, DefaultXRayMaterialPath);
	}
	if (!ScanMaterial)
	{
		if (!bLoggedXRayMaterialWarning)
		{
			bLoggedXRayMaterialWarning = true;
			UE_LOG(LogClaudeCore, Warning,
				TEXT("[ClcEagleEye] XRay scan material not found (configured=%s, default=%s)"),
				*Config->XRayScanMaterial.ToString(), DefaultXRayMaterialPath);
		}
		return;
	}
	XRayScanMID = UMaterialInstanceDynamic::Create(ScanMaterial, this);
	if (!XRayScanMID) return;

	float MinZ = FLT_MAX;
	float MaxZ = -FLT_MAX;

	// 范围内商人 clone 一份 Mesh 并应用扫描 MID（只扫商人，不扫原石）
	for (TActorIterator<AClcMerchant> It(World); It; ++It)
	{
		AClcMerchant* Merchant = *It;
		if (!IsValid(Merchant) || Merchant->IsActorBeingDestroyed()) continue;
		if (FVector::DistSquared(Merchant->GetActorLocation(), Center) > RadiusSq) continue;
		CloneXRayMeshes(Merchant, MinZ, MaxZ);
	}

	if (XRayCloneComponents.Num() == 0 || MinZ == FLT_MAX)
	{
		DestroyActiveXRayScan();
		return;
	}

	XRayScanCenter = Center;
	XRayBottomZ = MinZ;
	XRayTopZ = MaxZ;
	XRayScanDuration = FMath::Max(0.01f, Config->ScanDuration);
	XRayScanTimer = 0.0f;
	bXRayActive = true;

	// 扫描盒要覆盖整个扫描区域——材质默认 ScanBoxSize=300cm 只覆盖玩家近处，
	// 范围内远处的商人会落在盒外而不显示。ScanBoxSize 的 XY 用 ScanRadius*2，
	// 无论材质把 Size 当半尺寸还是全尺寸，都能覆盖整个扫描半径范围。
	const float ScanBoxXY = FMath::Max(Config->ScanRadius * 2.0f, 600.0f);
	constexpr float ScanBoxHalfZ = 40.0f;
	XRayScanMID->SetVectorParameterValue(
		FName(TEXT("Scan Box Size")),
		FLinearColor(ScanBoxXY, ScanBoxXY, ScanBoxHalfZ, 1.0f));

	UE_LOG(LogClaudeCore, Log,
		TEXT("[ClcEagleEye] XRay scan started: clones=%d, bottomZ=%.1f, topZ=%.1f, duration=%.2f"),
		XRayCloneComponents.Num(), XRayBottomZ, XRayTopZ, XRayScanDuration);

	// 诊断：扫描材质关键参数默认值（判断是否 ScanBoxSize/Opacity 默认导致不可见）
	{
		FLinearColor DefBoxSize(ForceInit), DefBoxOrigin(ForceInit);
		float DefOpacity = -1.f, DefFalloff = -1.f;
		XRayScanMID->GetVectorParameterValue(FName(TEXT("Scan Box Size")), DefBoxSize);
		XRayScanMID->GetVectorParameterValue(FName(TEXT("Scan Box Origin")), DefBoxOrigin);
		XRayScanMID->GetScalarParameterValue(FName(TEXT("Opacity")), DefOpacity);
		XRayScanMID->GetScalarParameterValue(FName(TEXT("Scan Edge Falloff")), DefFalloff);
		UE_LOG(LogClaudeCore, Log,
			TEXT("[ClcEagleEye] XRay MID defaults: ScanBoxSize=(%.2f,%.2f,%.2f,%.2f), ScanBoxOrigin=(%.2f,%.2f,%.2f,%.2f), Opacity=%.3f, ScanEdgeFalloff=%.3f"),
			DefBoxSize.R, DefBoxSize.G, DefBoxSize.B, DefBoxSize.A,
			DefBoxOrigin.R, DefBoxOrigin.G, DefBoxOrigin.B, DefBoxOrigin.A,
			DefOpacity, DefFalloff);
	}
	// 诊断：第一个 clone mesh 渲染摘要
	if (XRayCloneComponents.Num() > 0)
	{
		if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(XRayCloneComponents[0]))
		{
			UStaticMesh* M = SMC->GetStaticMesh();
			UE_LOG(LogClaudeCore, Log,
				TEXT("[ClcEagleEye] XRay clone[0]: mesh=%s, naniteDisallowed=%d, materials=%d, hiddenInGame=%d, bounds=%s"),
				M ? *M->GetName() : TEXT("null"),
				(int32)SMC->IsDisallowNanite(),
				SMC->GetNumMaterials(),
				(int32)SMC->bHiddenInGame,
				*SMC->Bounds.ToString());
		}
	}
}

void UClcEagleEyeComponent::CloneXRayMeshes(AActor* TargetActor, float& OutMinZ, float& OutMaxZ)
{
	if (!TargetActor || !XRayScanMID) return;

	// Static Mesh clone
	TArray<UStaticMeshComponent*> StaticComps;
	TargetActor->GetComponents<UStaticMeshComponent>(StaticComps, false);
	for (UStaticMeshComponent* SourceMesh : StaticComps)
	{
		if (!SourceMesh || !SourceMesh->GetStaticMesh()) continue;

		UStaticMeshComponent* Clone = NewObject<UStaticMeshComponent>(TargetActor);
		Clone->SetStaticMesh(SourceMesh->GetStaticMesh());
		// 复制体从出生即禁 Nanite——Translucent 扫描材质在 Nanite 下会被渲染器拒绝
		Clone->bDisallowNanite = true;
		Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Clone->SetGenerateOverlapEvents(false);
		Clone->CastShadow = false;
		const int32 NumMats = SourceMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			Clone->SetMaterial(i, XRayScanMID);
		}
		Clone->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Clone->RegisterComponent();
		XRayCloneComponents.Add(Clone);

		const FBoxSphereBounds Bounds = SourceMesh->Bounds;
		OutMinZ = FMath::Min(OutMinZ, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		OutMaxZ = FMath::Max(OutMaxZ, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}

	// Skeletal Mesh clone
	TArray<USkeletalMeshComponent*> SkelComps;
	TargetActor->GetComponents<USkeletalMeshComponent>(SkelComps, false);
	for (USkeletalMeshComponent* SourceMesh : SkelComps)
	{
		if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset()) continue;

		USkeletalMeshComponent* Clone = NewObject<USkeletalMeshComponent>(TargetActor);
		Clone->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
		// clone 作为源骨骼的姿态跟随者，镜像源动画，避免 t-pose
		Clone->SetLeaderPoseComponent(SourceMesh);
		Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Clone->SetGenerateOverlapEvents(false);
		Clone->CastShadow = false;
		const int32 NumMats = SourceMesh->GetNumMaterials();
		for (int32 i = 0; i < NumMats; ++i)
		{
			Clone->SetMaterial(i, XRayScanMID);
		}
		Clone->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Clone->RegisterComponent();
		XRayCloneComponents.Add(Clone);

		const FBoxSphereBounds Bounds = SourceMesh->Bounds;
		OutMinZ = FMath::Min(OutMinZ, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		OutMaxZ = FMath::Max(OutMaxZ, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}
}

void UClcEagleEyeComponent::DestroyActiveXRayScan()
{
	for (TObjectPtr<UMeshComponent>& Comp : XRayCloneComponents)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	XRayCloneComponents.Empty();
	XRayScanMID = nullptr;
	bXRayActive = false;
	XRayScanTimer = 0.0f;
}

void UClcEagleEyeComponent::CleanupScanEffect()
{
	bScanActive = false;
	ScanEndTimer = 0.0f;

	if (ScanPostProcessComponent)
	{
		ScanPostProcessComponent->DestroyComponent();
		ScanPostProcessComponent = nullptr;
	}
	ScanMID = nullptr;
}

bool UClcEagleEyeComponent::IsInExclusiveFlow() const
{
	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn) return true;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return true;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return true;

	if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
	{
		if (KP->IsInExclusiveFlow()) return true;
	}
	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
	{
		if (BP->IsBackpackOpen()) return true;
	}
	return false;
}

void UClcEagleEyeComponent::ActivateEagleEye()
{
	if (!Config) return;

	if (IsInExclusiveFlow()) return;

	if (bCoolingDown)
	{
		UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcEagleEye] On cooldown, can't activate."));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	const FVector Center = Owner->GetActorLocation();
	const float RadiusSq = FMath::Square(FMath::Max(0.0f, Config->ScanRadius));

	if (MarketSubsystem)
	{
		for (const auto& StallPtr : MarketSubsystem->GetStalls())
		{
			AClcStoneStall* Stall = StallPtr.Get();
			if (!Stall) continue;

			AClcMerchant* Merchant = Stall->GetMerchant();
			if (!Merchant) continue;

			if (FVector::DistSquared(Merchant->GetActorLocation(), Center) <= RadiusSq)
			{
				Merchant->ShowBubble(FMath::Max(0.0f, Config->ActiveDuration));
			}
		}
	}

	StartOrRestartScanPulse(Center);
	StartOrRestartXRayScan(Center);

	CooldownTimer = FMath::Max(0.0f, Config->CooldownDuration);
	bCoolingDown = CooldownTimer > 0.0f;
}

void UClcEagleEyeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 首帧 PC 就绪后注册常驻 Q 提示（BeginPlay 时角色可能尚未被 Possess）
	if (EagleEyePromptHandle == 0)
	{
		if (AActor* Owner = GetOwner())
		{
			if (APawn* Pawn = Cast<APawn>(Owner))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					if (ULocalPlayer* LP = PC->GetLocalPlayer())
					{
						if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
						{
							EagleEyePromptHandle = KP->RegisterKeyPrompt(
								EKeys::Q,
								NSLOCTEXT("ClcEagleEye", "EagleEyePromptLabel", "鹰眼"),
								FName("EagleEye"), 50);
						}
					}
				}
			}
		}
	}

	// 注：不在此禁用 Tick——ActivateEagleEye 由蓝图异步触发，Tick 启停时序与按键不同步会导致
	// 间歇性"偶尔没效果"。Tick 一直运行的开销极小（几个 float 比较），保持稳定优先。
	if (!Config) return;

	// 仅走玩家侧 CD；激活态与残留计时已下沉到每个商人自己的 Tick。
	if (bCoolingDown)
	{
		CooldownTimer -= DeltaTime;
		if (CooldownTimer <= 0.0f)
		{
			bCoolingDown = false;
		}
	}

	// 扫描视觉销毁计时：材质自己按游戏时间扩散并淡出，这里只负责到点清理后处理组件
	if (bScanActive)
	{
		ScanEndTimer -= DeltaTime;
		if (ScanEndTimer <= 0.0f)
		{
			CleanupScanEffect();
		}
	}

	if (bXRayActive && XRayScanMID)
	{
		XRayScanTimer += DeltaTime;
		float Alpha = XRayScanDuration > 0.0f ? XRayScanTimer / XRayScanDuration : 1.0f;
		if (Alpha >= 1.0f) Alpha = 1.0f;

		// 扫描盒原点 Z 从底扫到顶；XY 固定在按 Q 的中心，材质按 Box Origin+Size 渲染三角网格
		const float Z = FMath::Lerp(XRayBottomZ, XRayTopZ, Alpha);
		XRayScanMID->SetVectorParameterValue(
			FName(TEXT("Scan Box Origin")),
			FLinearColor(XRayScanCenter.X, XRayScanCenter.Y, Z, 1.0f));

		if (Alpha >= 1.0f)
		{
			DestroyActiveXRayScan();
		}
	}
}

void UClcEagleEyeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底注销按键提示（Owner/PC 可能已失效，子系统 Deinitialize 会统一清理）
	if (EagleEyePromptHandle != 0)
	{
		if (AActor* Owner = GetOwner())
		{
			if (APawn* Pawn = Cast<APawn>(Owner))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					if (ULocalPlayer* LP = PC->GetLocalPlayer())
					{
						if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
						{
							KP->UnregisterKeyPrompt(EagleEyePromptHandle);
						}
					}
				}
			}
		}
		EagleEyePromptHandle = 0;
	}

	CleanupScanEffect();
	DestroyActiveXRayScan();
	Super::EndPlay(EndPlayReason);
}
