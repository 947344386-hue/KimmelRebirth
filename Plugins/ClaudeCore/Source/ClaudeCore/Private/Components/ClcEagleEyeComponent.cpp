// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcEagleEyeComponent.h"
#include "ClcLog.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
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
	// 本组件 Tick 只做：首帧 Q 提示注册（成功后短路）、CD 倒计时、扫描销毁计时——均低频逻辑，
	// 0.2s 间隔足够，避免每帧轮询首帧注册和 float 递减。
	PrimaryComponentTick.TickInterval = 0.2f;
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
	TArray<AActor*> SeeThroughTargets;
	TArray<AActor*> OccludedTargets;

	if (Config->ResponseRules.IsEmpty())
	{
		for (TActorIterator<AClcMerchant> It(World); It; ++It)
		{
			AClcMerchant* Merchant = *It;
			if (!IsValid(Merchant) || Merchant->IsActorBeingDestroyed() || Merchant->IsHidden()) continue;
			if (FVector::DistSquared(Merchant->GetActorLocation(), Center) > RadiusSq) continue;
			SeeThroughTargets.Add(Merchant);
		}
	}
	else
	{
		TSet<AActor*> ProcessedActors;
		for (const FClcEagleEyeResponseRule& Rule : Config->ResponseRules)
		{
			UClass* ActorClass = Rule.ActorClass.Get();
			if (!ActorClass) continue;

			for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
			{
				AActor* TargetActor = *It;
				if (ProcessedActors.Contains(TargetActor)) continue;
				ProcessedActors.Add(TargetActor);
				if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed() || TargetActor->IsHidden()) continue;
				if (FVector::DistSquared(TargetActor->GetActorLocation(), Center) > RadiusSq) continue;

				const EClcEagleEyeResponseMode Mode = ResolveResponseMode(TargetActor, Rule.Mode);
				if (Mode == EClcEagleEyeResponseMode::SeeThrough)
				{
					SeeThroughTargets.Add(TargetActor);
				}
				else
				{
					OccludedTargets.Add(TargetActor);
				}
			}
		}
	}

	static const TCHAR* DefaultSeeThroughMaterialPath =
		TEXT("/Game/ScanFX/Materials/Instances/MI_ScanFX_TriangleScanner.MI_ScanFX_TriangleScanner");

	auto CreateScanMID = [this](const TSoftObjectPtr<UMaterialInterface>& MaterialRef,
		const TCHAR* FallbackPath, bool& bLoggedWarning, const TCHAR* Label) -> UMaterialInstanceDynamic*
	{
		UMaterialInterface* ScanMaterial = MaterialRef.IsNull() ? nullptr : MaterialRef.LoadSynchronous();
		if (!ScanMaterial && FallbackPath)
		{
			ScanMaterial = LoadObject<UMaterialInterface>(nullptr, FallbackPath);
		}
		if (!ScanMaterial)
		{
			if (!bLoggedWarning)
			{
				bLoggedWarning = true;
				UE_LOG(LogClaudeCore, Warning,
					TEXT("[ClcEagleEye] %s scan material not found (configured=%s)"),
					Label, *MaterialRef.ToString());
			}
			return nullptr;
		}
		return UMaterialInstanceDynamic::Create(ScanMaterial, this);
	};

	if (!SeeThroughTargets.IsEmpty())
	{
		SeeThroughScanMID = CreateScanMID(Config->XRayScanMaterial, DefaultSeeThroughMaterialPath,
			bLoggedSeeThroughMaterialWarning, TEXT("See-through"));
	}
	if (!OccludedTargets.IsEmpty())
	{
		OccludedScanMID = CreateScanMID(Config->OccludedScanMaterial, nullptr,
			bLoggedOccludedMaterialWarning, TEXT("Occluded"));
	}

	float SeeThroughMinZ = FLT_MAX;
	float SeeThroughMaxZ = -FLT_MAX;
	float OccludedMinZ = FLT_MAX;
	float OccludedMaxZ = -FLT_MAX;
	int32 SeeThroughCloneCount = 0;
	int32 OccludedCloneCount = 0;

	if (SeeThroughScanMID)
	{
		for (AActor* TargetActor : SeeThroughTargets)
		{
			CloneXRayMeshes(TargetActor, SeeThroughScanMID, SeeThroughMinZ, SeeThroughMaxZ,
				SeeThroughCloneCount);
		}
	}
	if (OccludedScanMID)
	{
		for (AActor* TargetActor : OccludedTargets)
		{
			CloneXRayMeshes(TargetActor, OccludedScanMID, OccludedMinZ, OccludedMaxZ,
				OccludedCloneCount);
		}
	}

	auto ShowScanResultToast = [this](int32 SeeThroughCount, int32 OccludedCount)
	{
		APawn* Pawn = Cast<APawn>(GetOwner());
		APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		UClcLogToastSubsystem* LogToast = PC ? ClcGetLogToast(PC) : nullptr;
		if (!LogToast) return;

		FString Message;
		FLinearColor Color = FLinearColor(0.1f, 0.85f, 1.0f);
		if (SeeThroughCount > 0 && OccludedCount > 0)
		{
			Message = FString::Printf(TEXT("鹰眼洞察：已看穿 %d 个目标，另有 %d 个目标无法看穿"),
				SeeThroughCount, OccludedCount);
		}
		else if (SeeThroughCount > 0)
		{
			Message = FString::Printf(TEXT("鹰眼洞察：已看穿 %d 个目标"), SeeThroughCount);
		}
		else if (OccludedCount > 0)
		{
			Message = FString::Printf(TEXT("鹰眼洞察：%d 个目标无法看穿"), OccludedCount);
			Color = FLinearColor::Yellow;
		}
		else
		{
			Message = TEXT("鹰眼扫描：附近未发现可洞察目标");
			Color = FLinearColor::Yellow;
		}

		LogToast->AddLog(Message, 2.5f, Color);
	};

	// 按唯一 Actor 计数（每个 Actor 可能含多个 Mesh 组件，每个 Mesh 产生一个 clone）——
	// 飘字口径：有至少一个 clone 的 Actor 数，而非 clone 组件数。
	{
		TSet<AActor*> SeeThroughSet(SeeThroughTargets);
		TSet<AActor*> OccludedSet(OccludedTargets);
		TSet<AActor*> SeeThroughActors, OccludedActors;
		for (const TObjectPtr<UMeshComponent>& Comp : XRayCloneComponents)
		{
			if (!IsValid(Comp)) continue;
			AActor* Owner = Comp->GetOwner();
			if (!Owner) continue;
			if (SeeThroughSet.Contains(Owner))
				SeeThroughActors.Add(Owner);
			else if (OccludedSet.Contains(Owner))
				OccludedActors.Add(Owner);
		}
		ShowScanResultToast(SeeThroughActors.Num(), OccludedActors.Num());
	}
	if (XRayCloneComponents.IsEmpty())
	{
		DestroyActiveXRayScan();
		return;
	}

	if (SeeThroughCloneCount > 0 && SeeThroughMinZ != FLT_MAX)
	{
		SeeThroughBottomZ = SeeThroughMinZ;
		SeeThroughTopZ = SeeThroughMaxZ;
	}
	else
	{
		SeeThroughScanMID = nullptr;
	}

	if (OccludedCloneCount > 0 && OccludedMinZ != FLT_MAX)
	{
		OccludedBottomZ = OccludedMinZ;
		OccludedTopZ = OccludedMaxZ;
	}
	else
	{
		OccludedScanMID = nullptr;
	}

	XRayScanCenter = Center;
	XRayScanDuration = FMath::Max(0.01f, Config->ScanDuration);
	XRayScanTimer = 0.0f;
	bXRayActive = true;

	const float ScanBoxXY = FMath::Max(Config->ScanRadius * 2.0f, 600.0f);
	constexpr float ScanBoxHalfZ = 40.0f;
	auto ConfigureScanBox = [ScanBoxXY, ScanBoxHalfZ](UMaterialInstanceDynamic* ScanMID)
	{
		if (!ScanMID) return;
		ScanMID->SetVectorParameterValue(
			FName(TEXT("Scan Box Size")),
			FLinearColor(ScanBoxXY, ScanBoxXY, ScanBoxHalfZ, 1.0f));
	};
	ConfigureScanBox(SeeThroughScanMID);
	ConfigureScanBox(OccludedScanMID);

	UE_LOG(LogClaudeCore, Log,
		TEXT("[ClcEagleEye] Mesh scan started: seeThroughActors=%d, seeThroughClones=%d, occludedActors=%d, occludedClones=%d, duration=%.2f"),
		SeeThroughTargets.Num(), SeeThroughCloneCount, OccludedTargets.Num(), OccludedCloneCount, XRayScanDuration);
}

EClcEagleEyeResponseMode UClcEagleEyeComponent::ResolveResponseMode(const AActor* TargetActor,
	EClcEagleEyeResponseMode DefaultMode) const
{
	if (!TargetActor) return DefaultMode;

	static const FName SeeThroughTag(TEXT("EagleEyeSeeThrough"));
	static const FName OccludedTag(TEXT("EagleEyeOccluded"));
	const bool bSeeThroughOverride = TargetActor->ActorHasTag(SeeThroughTag);
	const bool bOccludedOverride = TargetActor->ActorHasTag(OccludedTag);

	if (bSeeThroughOverride && bOccludedOverride)
	{
		UE_LOG(LogClaudeCore, Warning,
			TEXT("[ClcEagleEye] Actor %s has both response tags; EagleEyeOccluded takes precedence."),
			*TargetActor->GetName());
		return EClcEagleEyeResponseMode::Occluded;
	}
	if (bOccludedOverride) return EClcEagleEyeResponseMode::Occluded;
	if (bSeeThroughOverride) return EClcEagleEyeResponseMode::SeeThrough;
	return DefaultMode;
}

void UClcEagleEyeComponent::CloneXRayMeshes(AActor* TargetActor, UMaterialInstanceDynamic* TargetMID,
	float& OutMinZ, float& OutMaxZ, int32& OutCloneCount)
{
	if (!IsValid(TargetActor) || !TargetMID) return;

	TArray<UStaticMeshComponent*> StaticComps;
	TargetActor->GetComponents<UStaticMeshComponent>(StaticComps, false);
	for (UStaticMeshComponent* SourceMesh : StaticComps)
	{
		if (!IsValid(SourceMesh) || !SourceMesh->GetStaticMesh() || !SourceMesh->IsRegistered()
			|| !SourceMesh->IsVisible() || SourceMesh->bHiddenInGame)
		{
			continue;
		}

		UStaticMeshComponent* Clone = NewObject<UStaticMeshComponent>(TargetActor, NAME_None, RF_Transient);
		Clone->SetStaticMesh(SourceMesh->GetStaticMesh());
		Clone->bDisallowNanite = true;
		Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Clone->SetGenerateOverlapEvents(false);
		Clone->CastShadow = false;
		const int32 NumMats = SourceMesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMats; ++Index)
		{
			Clone->SetMaterial(Index, TargetMID);
		}
		Clone->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Clone->RegisterComponent();
		XRayCloneComponents.Add(Clone);
		++OutCloneCount;

		const FBoxSphereBounds Bounds = SourceMesh->Bounds;
		OutMinZ = FMath::Min(OutMinZ, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		OutMaxZ = FMath::Max(OutMaxZ, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}

	TArray<USkeletalMeshComponent*> SkelComps;
	TargetActor->GetComponents<USkeletalMeshComponent>(SkelComps, false);
	for (USkeletalMeshComponent* SourceMesh : SkelComps)
	{
		if (!IsValid(SourceMesh) || !SourceMesh->GetSkeletalMeshAsset() || !SourceMesh->IsRegistered()
			|| !SourceMesh->IsVisible() || SourceMesh->bHiddenInGame)
		{
			continue;
		}

		USkeletalMeshComponent* Clone = NewObject<USkeletalMeshComponent>(TargetActor, NAME_None, RF_Transient);
		Clone->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
		Clone->SetLeaderPoseComponent(SourceMesh);
		Clone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Clone->SetGenerateOverlapEvents(false);
		Clone->CastShadow = false;
		const int32 NumMats = SourceMesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMats; ++Index)
		{
			Clone->SetMaterial(Index, TargetMID);
		}
		Clone->AttachToComponent(SourceMesh, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Clone->RegisterComponent();
		XRayCloneComponents.Add(Clone);
		++OutCloneCount;

		const FBoxSphereBounds Bounds = SourceMesh->Bounds;
		OutMinZ = FMath::Min(OutMinZ, Bounds.Origin.Z - Bounds.BoxExtent.Z);
		OutMaxZ = FMath::Max(OutMaxZ, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}
}

void UClcEagleEyeComponent::DestroyActiveXRayScan()
{
	for (TObjectPtr<UMeshComponent>& Comp : XRayCloneComponents)
	{
		if (IsValid(Comp))
		{
			Comp->DestroyComponent();
		}
	}
	XRayCloneComponents.Empty();
	SeeThroughScanMID = nullptr;
	OccludedScanMID = nullptr;
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

	if (bXRayActive && (SeeThroughScanMID || OccludedScanMID))
	{
		XRayScanTimer += DeltaTime;
		float Alpha = XRayScanDuration > 0.0f ? XRayScanTimer / XRayScanDuration : 1.0f;
		if (Alpha >= 1.0f) Alpha = 1.0f;

		if (SeeThroughScanMID)
		{
			const float Z = FMath::Lerp(SeeThroughBottomZ, SeeThroughTopZ, Alpha);
			SeeThroughScanMID->SetVectorParameterValue(
				FName(TEXT("Scan Box Origin")),
				FLinearColor(XRayScanCenter.X, XRayScanCenter.Y, Z, 1.0f));
		}

		if (OccludedScanMID)
		{
			const float Z = FMath::Lerp(OccludedTopZ, OccludedBottomZ, Alpha);
			OccludedScanMID->SetVectorParameterValue(
				FName(TEXT("Scan Box Origin")),
				FLinearColor(XRayScanCenter.X, XRayScanCenter.Y, Z, 1.0f));
		}

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
