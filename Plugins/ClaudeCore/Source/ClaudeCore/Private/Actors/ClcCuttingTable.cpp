// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcCuttingTable.h"
#include "Actors/ClcCuttingStone.h"
#include "ClcLog.h"
#include "Components/ClcInteractionIndicator.h"
#include "Components/ClcInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RandomStream.h"
#include "Math/RotationMatrix.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "Data/ClcStoneConfig.h"
#include "UI/ClcBackpackWidget.h"
#include "UI/ClcCuttingTableHUD.h"
#include "UI/ClcGoldFlyWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AClcCuttingTable::AClcCuttingTable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;

	TableRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TableRoot"));
	RootComponent = TableRoot;

	TableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableMesh"));
	TableMesh->SetupAttachment(TableRoot);
	TableMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TableMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	TableMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(TableRoot);
	TriggerSphere->InitSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	StoneSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnPoint"));
	StoneSpawnPoint->SetupAttachment(TableRoot);
	StoneSpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	BladePoint = CreateDefaultSubobject<USceneComponent>(TEXT("BladePoint"));
	BladePoint->SetupAttachment(TableRoot);
	BladePoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(BladePoint);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(StoneSpawnPoint);
	CameraArm->TargetArmLength = 450.0f;
	CameraArm->SetRelativeRotation(FRotator(-85.0f, 0.0f, 0.0f));
	CameraArm->bDoCollisionTest = false;
	CameraArm->bUsePawnControlRotation = false;
	CameraArm->bInheritPitch = true;
	CameraArm->bInheritYaw = true;
	CameraArm->bInheritRoll = true;

	WorkCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("WorkCamera"));
	WorkCamera->SetupAttachment(CameraArm);

	// ---- 左右侧视相机（挂 TableRoot，下刀电影感机位；位置/旋转在 BP 微调） ----
	LeftCutCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("LeftCutCamera"));
	LeftCutCamera->SetupAttachment(TableRoot);
	LeftCutCamera->SetRelativeLocation(FVector(0.0f, -250.0f, 120.0f));
	LeftCutCamera->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	LeftCutCamera->SetMobility(EComponentMobility::Movable);
	LeftCutCamera->SetActive(false);

	RightCutCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("RightCutCamera"));
	RightCutCamera->SetupAttachment(TableRoot);
	RightCutCamera->SetRelativeLocation(FVector(0.0f, 250.0f, 120.0f));
	RightCutCamera->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	RightCutCamera->SetMobility(EComponentMobility::Movable);
	RightCutCamera->SetActive(false);

	// ---- 桌面承接碰撞（承接物理掉落的切下块；不影响现有 TableMesh 的 QueryOnly） ----
	CatchBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CatchBox"));
	CatchBox->SetupAttachment(TableRoot);
	CatchBox->SetBoxExtent(FVector(120.0f, 200.0f, 5.0f));
	CatchBox->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	CatchBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CatchBox->SetCollisionObjectType(ECC_WorldStatic);
	CatchBox->SetCollisionResponseToAllChannels(ECR_Block);
	CatchBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CatchBox->SetGenerateOverlapEvents(false);

	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
	HUDWidgetClass = UClcCuttingTableHUD::StaticClass();

	// ---- 自适应补光（挂到 StoneSpawnPoint 跟随石头/相机；位置/锥角/颜色在 BP 上调） ----
	FillLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(StoneSpawnPoint);
	FillLight->SetRelativeLocation(FVector(70.0f, 0.0f, 130.0f));
	FillLight->SetRelativeRotation(FRotator(-65.0f, 0.0f, 0.0f));
	FillLight->SetIntensity(0.0f);
	FillLight->SetLightColor(FLinearColor(1.0f, 0.96f, 0.88f));
	FillLight->SetInnerConeAngle(60.0f);
	FillLight->SetOuterConeAngle(80.0f);
	FillLight->SetAttenuationRadius(500.0f);
	FillLight->SetCastShadows(false);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetVisibility(false);

	// ---- 切刀瞄准线贴花（投影到石头表面显示切割线 + 端帽，颜色按四态动态调） ----
	AimDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("AimDecal"));
	AimDecal->SetupAttachment(TableRoot);
	AimDecal->SetRelativeLocation(FVector::ZeroVector);
	// 与擦石预览一致：Decal 默认沿 +X 投影，Pitch 90° 后沿组件 -Z 投向石头顶面。
	// 运行时 UpdateAimDecalTransform 会按切线方向和石头 Bounds 覆盖世界变换。
	AimDecal->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	AimDecal->DecalSize = FVector(100.0f, 120.0f, 120.0f); // (投影深度, 宽, 高)
	AimDecal->SetMobility(EComponentMobility::Movable);
	AimDecal->SetVisibility(false);
}

void AClcCuttingTable::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->SetSphereRadius(TriggerRadius);
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcCuttingTable::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcCuttingTable::OnTriggerEndOverlap);

	InteractionIndicator->InteractionRadius = TriggerRadius;
	InteractionIndicator->bSelectByProximity = false;
	InteractionIndicator->OnQueryCanSelect.BindDynamic(this, &AClcCuttingTable::QueryCanSelect);

	if (UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld()))
	{
		Durability->InitTool(EClcRepairableTool::Blade, BladeMaxDurability);
	}

	if (BladePoint)
	{
		BladePointInitialRelativeLocation = BladePoint->GetRelativeLocation();
	}
}

void AClcCuttingTable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 自适应补光：放在所有 early return 之前
	TickFillLight(DeltaTime);

	// Inactive 状态：F 键入口由 UClcInteractionComponent 统一路由到 OnInteract
	if (CurrentState == EClcCuttingTableState::Inactive)
	{
		// 只需判断范围内且按了 F → 组件已做；这里处理进入模式
		// OnInteract 内调 EnterCuttingMode
		return;
	}

	if (!CachedPC.IsValid()) return;

	const bool bExitDown = CachedPC->IsInputKeyDown(ExitKey);
	if (bExitDown && !bExitKeyPrev)
	{
		bExitKeyPrev = true;
		// 切石动画中禁止 Esc 退出——中途退出会导致石头残留在台面且不归还背包
		if (CurrentState == EClcCuttingTableState::CuttingCinematic)
		{
			return;
		}
		if (UClcBackpackSubsystem* Backpack = GetBackpack(); Backpack && Backpack->IsBackpackOpen())
		{
			Backpack->ToggleBackpack();
			SetCuttingInputMode();
		}
		else
		{
			ExitCuttingMode();
			return;
		}
	}
	else if (!bExitDown)
	{
		bExitKeyPrev = false;
	}

	HandleBackpackInput();

	if (CurrentState == EClcCuttingTableState::CuttingCinematic)
	{
		TickCutCinematic(DeltaTime);
		if (AimDecal) AimDecal->SetVisibility(false);
		HUDPushTimer -= DeltaTime;
		if (HUDPushTimer <= 0.0f)
		{
			PushHUDData();
			HUDPushTimer = HUDPushInterval;
		}
		return; // 循环中屏蔽 A/D 和再次 SpaceBar
	}

	if (CurrentState == EClcCuttingTableState::StoneOnBench)
	{
		UClcBackpackSubsystem* Backpack = GetBackpack();
		if (!Backpack || !Backpack->IsBackpackOpen())
		{
			ProcessCuttingInput(DeltaTime);
		}

		HUDPushTimer -= DeltaTime;
		if (HUDPushTimer <= 0.0f)
		{
			PushHUDData();
			HUDPushTimer = HUDPushInterval;
		}
	}
}

void AClcCuttingTable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CurrentState != EClcCuttingTableState::Inactive)
	{
		ExitCuttingMode();
	}

	DestroyHUD();
	DestroyCuttingStone();
	UnbindFromBackpackWidget();

	if (CutPieceCleanupHandle.IsValid() && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CutPieceCleanupHandle);
	}
	CleanupLastCutPiece();

	if (InteractionIndicator)
	{
		InteractionIndicator->OnQueryCanSelect.Unbind();
	}

	if (TriggerSphere)
	{
		TriggerSphere->OnComponentBeginOverlap.RemoveDynamic(this, &AClcCuttingTable::OnTriggerBeginOverlap);
		TriggerSphere->OnComponentEndOverlap.RemoveDynamic(this, &AClcCuttingTable::OnTriggerEndOverlap);
	}

	// F 键提示已由 UClcInteractionComponent 统一管理，无需本站注销

	Super::EndPlay(EndPlayReason);
}

FText AClcCuttingTable::GetInteractionPrompt() const
{
	return InteractionPrompt;
}

bool AClcCuttingTable::OnInteract(AActor* Interactor)
{
	if (CurrentState != EClcCuttingTableState::Inactive) return false;

	if (APawn* Pawn = Cast<APawn>(Interactor))
	{
		PlayerInRange = Pawn;
		CachePlayerRefs();
	}

	EnterCuttingMode();
	return CurrentState != EClcCuttingTableState::Inactive;
}

void AClcCuttingTable::EnterCuttingMode()
{
	if (!CachedPC.IsValid() || !CachedBackpack || CurrentState != EClcCuttingTableState::Inactive)
	{
		return;
	}

	// 升级门控：未购买「解石台」升级 → 不能进入
	if (UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld()))
	{
		if (!Durability->HasCuttingTable())
		{
			if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
			{
				Toast->AddLog(TEXT("尚未解锁解石台——请先在升级商店购买「解石台」升级"), 2.5f, FLinearColor::Yellow);
			}
			return;
		}
	}

	if (!HasEligibleStone())
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			const bool bEmpty = CachedBackpack->GetStones().IsEmpty();
			Toast->AddLog(bEmpty ? TEXT("背包空，无可解石的石头")
				: TEXT("背包中没有可解石的石头"), 2.2f, FLinearColor::Yellow);
		}
		return;
	}

	CurrentState = EClcCuttingTableState::AwaitingStone;
	InteractionIndicator->bHidden = true;

	// 隐藏常驻 UI，避免与解石台 HUD 冲突
	if (CachedPC.IsValid())
	{
		if (const ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			if (auto* QS = LP->GetSubsystem<UClcQuestSubsystem>())
				QS->SetTrackerVisible(false);
		}
	}
	if (CachedBackpack)
		CachedBackpack->SetHudVisible(false);

	OnEnterCuttingMode();

	if (UClcBackpackSubsystem* Backpack = GetBackpack())
	{
		if (!Backpack->IsBackpackOpen())
		{
			Backpack->ToggleBackpack();
		}
		BindToBackpackWidget();
	}

	bExitKeyPrev = CachedPC->IsInputKeyDown(ExitKey);
	bBackpackKeyPrev = CachedPC->IsInputKeyDown(BackpackKey);
	bCutKeyPrev = false;

	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		Toast->AddLog(TEXT("进入解石模式——选择一块原石"), 2.0f, FLinearColor(0.0f, 1.0f, 1.0f));
	}
}

void AClcCuttingTable::ExitCuttingMode()
{
	if (CurrentState == EClcCuttingTableState::Inactive) return;

	if (UClcBackpackSubsystem* Backpack = GetBackpack())
	{
		if (Backpack->IsBackpackOpen())
		{
			Backpack->ToggleBackpack();
		}
	}
	UnbindFromBackpackWidget();

	const bool bHadStone = CurrentState == EClcCuttingTableState::StoneOnBench;
	if (bHadStone)
	{
		RemoveStoneFromBench();
	}

	CurrentState = EClcCuttingTableState::Inactive;
	InteractionIndicator->bHidden = false;
	OnExitCuttingMode();
	SetCuttingInputMode();

	// 恢复常驻 UI
	if (CachedPC.IsValid())
	{
		if (const ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			if (auto* QS = LP->GetSubsystem<UClcQuestSubsystem>())
				QS->SetTrackerVisible(true);
		}
	}
	if (CachedBackpack)
		CachedBackpack->SetHudVisible(true);

	bExitKeyPrev = false;
	bBackpackKeyPrev = false;
	bCutKeyPrev = false;

	if (bHadStone)
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("解石进度已保存"), 2.0f, FLinearColor(0.0f, 1.0f, 1.0f));
		}
	}
}

void AClcCuttingTable::HandleBackpackInput()
{
	if (!CachedPC.IsValid()) return;

	const bool bBackpackDown = CachedPC->IsInputKeyDown(BackpackKey);
	if (bBackpackDown && !bBackpackKeyPrev)
	{
		if (UClcBackpackSubsystem* Backpack = GetBackpack())
		{
			Backpack->ToggleBackpack();
			if (Backpack->IsBackpackOpen())
			{
				BindToBackpackWidget();
			}
			else
			{
				SetCuttingInputMode();
			}
		}
	}
	bBackpackKeyPrev = bBackpackDown;
}

void AClcCuttingTable::ProcessCuttingInput(float DeltaTime)
{
	if (!CuttingStone || !CachedPC.IsValid()) return;

	float MoveDirection = 0.0f;
	if (CachedPC->IsInputKeyDown(MoveLeftKey)) MoveDirection -= 1.0f;
	if (CachedPC->IsInputKeyDown(MoveRightKey)) MoveDirection += 1.0f;

	if (!FMath::IsNearlyZero(MoveDirection))
	{
		StoneOffset = FMath::Clamp(
			StoneOffset + MoveDirection * StoneMoveSpeed * DeltaTime,
			-MovementRange, MovementRange);
		CuttingStone->SetActorLocation(StoneBaseWorldLocation + MovementAxisWorld * StoneOffset);
	}

	const bool bCutDown = CachedPC->IsInputKeyDown(CutKey);
	if (bCutDown && !bCutKeyPrev)
	{
		if (IsRemainingStoneInStandardRange())
		{
			SellRemainingStone();
		}
		else
		{
			ExecuteBladeDrop();
		}
	}
	bCutKeyPrev = bCutDown;
}

bool AClcCuttingTable::ExecuteBladeDrop()
{
	return StartCutCinematic();
}

bool AClcCuttingTable::StartCutCinematic()
{
	if (!CuttingStone || !CachedPC.IsValid()) return false;
	if (CurrentState != EClcCuttingTableState::StoneOnBench) return false;
	if (BladePhase != EBladePhase::Idle) return false;

	// 机械性检查统一走 CanCutNow（BladePoint/穿透平面/刀片耐久）——避免两套逻辑分叉
	if (!CanCutNow()) return false;

	FClcStoneRuntimeData StoneData;
	if (!CuttingStone->GetStoneData(StoneData)) return false;
	if (StoneData.bHaggleResolved)
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("这块石头已锁价，不能继续解石"), 2.2f, FLinearColor(1.0f, 0.5f, 0.2f));
		}
		return false;
	}

	// ---- 缓存刀口平面（升刀动画会移动 BladePoint，预判和实际切割必须共用同一平面） ----
	// CanCutAtWorldPlane 已由 CanCutNow() 用同一组值验证过，这里只管缓存
	PendingCutPlanePoint = BladePoint->GetComponentLocation();
	PendingCutPlaneNormal = MovementAxisWorld;

	// ---- 预判切走侧（不修改场），据此选相机 ----
	bool bWillRemoveNegative = false;
	if (!CuttingStone->PredictCutSide(PendingCutPlanePoint, PendingCutPlaneNormal, bWillRemoveNegative))
	{
		return false;
	}
	bCutRemovedNegativeSide = bWillRemoveNegative;

	// ---- 按真实几何方向选择相机 ----
	// 负侧在 -MovementAxisWorld，正侧在 +MovementAxisWorld；比较两台相机实际位置在切走方向上的投影。
	// 不再使用 Stone Actor 原点，因为多刀后剩余石体中心会偏移，Actor 原点不能代表当前几何中心。
	const FVector CutAwayDirection =
		(bWillRemoveNegative ? -PendingCutPlaneNormal : PendingCutPlaneNormal).GetSafeNormal();
	const float LeftCameraScore = LeftCutCamera
		? FVector::DotProduct(LeftCutCamera->GetComponentLocation() - PendingCutPlanePoint, CutAwayDirection)
		: -BIG_NUMBER;
	const float RightCameraScore = RightCutCamera
		? FVector::DotProduct(RightCutCamera->GetComponentLocation() - PendingCutPlanePoint, CutAwayDirection)
		: -BIG_NUMBER;
	UCameraComponent* WatchCam = LeftCameraScore >= RightCameraScore
		? LeftCutCamera
		: RightCutCamera;
	ActiveCutCamera = WatchCam;

	// 先全清 Active 防残留，再激活目标相机
	if (WorkCamera) WorkCamera->SetActive(false);
	if (LeftCutCamera) LeftCutCamera->SetActive(false);
	if (RightCutCamera) RightCutCamera->SetActive(false);
	if (WatchCam) WatchCam->SetActive(true);
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(this, 0.3f);
	}

	// ---- 进入切割电影感循环 ----
	CurrentState = EClcCuttingTableState::CuttingCinematic;
	bCutExecutedThisCycle = false;
	EnterBladePhase(EBladePhase::LiftBlade);
	return true;
}

void AClcCuttingTable::EnterBladePhase(EBladePhase NewPhase)
{
	BladePhase = NewPhase;
	PhaseTimer = 0.0f;
	if (NewPhase == EBladePhase::Cut)
	{
		ExecuteCutDuringCinematic();
	}
	else if (NewPhase == EBladePhase::ShowCutFace)
	{
		ShowCutFaceInfo();
	}
	else if (NewPhase == EBladePhase::Finish)
	{
		FinishCutCinematic();
	}
}

void AClcCuttingTable::TickCutCinematic(float DeltaTime)
{
	PhaseTimer += DeltaTime;

	switch (BladePhase)
	{
	case EBladePhase::LiftBlade:
	{
		const float Alpha = FMath::Clamp(PhaseTimer / LiftDuration, 0.0f, 1.0f);
		const float CurrentLift = FMath::Lerp(0.0f, BladeLiftHeight, Alpha);
		if (BladePoint)
		{
			FVector RelLoc = BladePointInitialRelativeLocation;
			RelLoc.Z += CurrentLift;
			BladePoint->SetRelativeLocation(RelLoc);
		}
		if (BladeMesh)
		{
			const FQuat Spin = FQuat(BladeSpinAxis.GetSafeNormal(),
				FMath::DegreesToRadians(BladeSpinSpeed * DeltaTime));
			BladeMesh->AddRelativeRotation(Spin);
		}
		if (PhaseTimer >= LiftDuration)
		{
			EnterBladePhase(EBladePhase::Cut);
		}
		break;
	}
	case EBladePhase::Cut:
	{
		// ExecuteCut 已在 EnterBladePhase(Cut) 执行；此阶段仅等待物理稳定缓冲
		if (PhaseTimer >= CutDuration)
		{
			EnterBladePhase(EBladePhase::DescendBlade);
		}
		break;
	}
	case EBladePhase::DescendBlade:
	{
		const float Alpha = FMath::Clamp(PhaseTimer / DescendDuration, 0.0f, 1.0f);
		const float CurrentLift = FMath::Lerp(BladeLiftHeight, 0.0f, Alpha);
		if (BladePoint)
		{
			FVector RelLoc = BladePointInitialRelativeLocation;
			RelLoc.Z += CurrentLift;
			BladePoint->SetRelativeLocation(RelLoc);
		}
		if (BladeMesh)
		{
			const FQuat Spin = FQuat(BladeSpinAxis.GetSafeNormal(),
				FMath::DegreesToRadians(BladeSpinSpeed * DeltaTime));
			BladeMesh->AddRelativeRotation(Spin);
		}
		if (PhaseTimer >= DescendDuration)
		{
			EnterBladePhase(EBladePhase::ShowCutFace); // 停旋转
		}
		break;
	}
	case EBladePhase::ShowCutFace:
	{
		if (PhaseTimer >= ShowCutFaceDuration)
		{
			EnterBladePhase(EBladePhase::Finish);
		}
		break;
	}
	default: break;
	}
}

void AClcCuttingTable::ExecuteCutDuringCinematic()
{
	if (!CuttingStone || !CachedPC.IsValid() || bCutExecutedThisCycle) return;

	UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld());
	if (!Durability) return;
	const float CurrentDurability = Durability->GetDurability(EClcRepairableTool::Blade);

	int32 CutAwayTotal = 0;
	int32 CutAwayJade = 0;
	int32 CutAwayCrack = 0;
	int32 CutAwayImpurity = 0;

	// 用缓存的刀口平面（与预判/相机一致），不再强制切侧——让 ExecuteCut 在固定平面上重新
	// 自动比较两侧体积，避免升刀后 BladePoint 偏移导致预判与实际切割不一致。
	if (!CuttingStone->ExecuteCut(PendingCutPlanePoint, PendingCutPlaneNormal,
		false, CutAwayTotal, CutAwayJade, CutAwayCrack, CutAwayImpurity))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcCuttingTable][CutResult] ExecuteCut failed"));
		return;
	}
	bCutExecutedThisCycle = true;

	// 通知任务系统：解石完成 +1
	if (CachedPC.IsValid())
	{
		if (const ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
			{
				QS->NotifyObjectiveProgress(EClcQuestObjectiveType::CutStones, 1);
			}
		}
	}

	// 记录实际移除侧（ExecuteCut 内部自动选了较小侧，这里从 LastCutPieceWorldCenter 反推）
	const FVector CutPieceLoc = CuttingStone->GetCutPieceWorldLocation();
	const bool bActuallyRemovedNegOnRetry = FVector::DotProduct(
		CutPieceLoc - PendingCutPlanePoint, PendingCutPlaneNormal) < 0.0f;
	bCutRemovedNegativeSide = bActuallyRemovedNegOnRetry;

	const float VoxelVol = CuttingStone->GetVoxelField().VoxelVolume;
	const float CutVolume = static_cast<float>(CutAwayTotal) * VoxelVol;
	const int32 RemainingTotal = FMath::Max(1, static_cast<int32>(CuttingStone->GetVoxelField().GetRemainingVolume() / VoxelVol));
	const float CutRatio = CutAwayTotal > 0
		? static_cast<float>(CutAwayTotal) / static_cast<float>(CutAwayTotal + RemainingTotal)
		: 0.0f;

	// ---- 切下块物理掉落 ----
	UProceduralMeshComponent* OtherHalf = CuttingStone->GetLastOtherHalf();
	if (OtherHalf)
	{
		// UE 明确禁止 ComplexAsSimple 三角网格参与动态模拟。
		// 渲染仍用精确切片网格，物理改用本地包围盒的简单凸碰撞。
		OtherHalf->SetMobility(EComponentMobility::Movable);
		OtherHalf->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		OtherHalf->SetSimulatePhysics(false);
		OtherHalf->bUseComplexAsSimpleCollision = false;
		OtherHalf->bUseAsyncCooking = false;
		OtherHalf->SetCollisionProfileName(TEXT("PhysicsActor"));
		OtherHalf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		OtherHalf->SetCollisionObjectType(ECC_WorldDynamic);
		OtherHalf->SetCollisionResponseToAllChannels(ECR_Block);
		OtherHalf->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

		FBox LocalBounds(ForceInit);
		for (int32 SectionIndex = 0; SectionIndex < OtherHalf->GetNumSections(); ++SectionIndex)
		{
			if (const FProcMeshSection* Section = OtherHalf->GetProcMeshSection(SectionIndex))
			{
				for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
				{
					LocalBounds += FVector(Vertex.Position);
				}
			}
		}

		if (LocalBounds.IsValid)
		{
			const FVector Min = LocalBounds.Min;
			const FVector Max = LocalBounds.Max;
			TArray<FVector> ConvexVerts;
			ConvexVerts.Reserve(8);
			ConvexVerts.Add(FVector(Min.X, Min.Y, Min.Z));
			ConvexVerts.Add(FVector(Min.X, Min.Y, Max.Z));
			ConvexVerts.Add(FVector(Min.X, Max.Y, Min.Z));
			ConvexVerts.Add(FVector(Min.X, Max.Y, Max.Z));
			ConvexVerts.Add(FVector(Max.X, Min.Y, Min.Z));
			ConvexVerts.Add(FVector(Max.X, Min.Y, Max.Z));
			ConvexVerts.Add(FVector(Max.X, Max.Y, Min.Z));
			ConvexVerts.Add(FVector(Max.X, Max.Y, Max.Z));

			TArray<TArray<FVector>> ConvexMeshes;
			ConvexMeshes.Add(MoveTemp(ConvexVerts));
			OtherHalf->SetCollisionConvexMeshes(ConvexMeshes);
			OtherHalf->SetSimulatePhysics(true);
			OtherHalf->SetEnableGravity(true);
			OtherHalf->WakeRigidBody(NAME_None);

			// ---- 推出方向：朝相机水平方向 + 随机扰动 ----
			FVector CameraDirection =
				ActiveCutCamera.IsValid()
				? ActiveCutCamera->GetComponentLocation() - OtherHalf->GetComponentLocation()
				: (bCutRemovedNegativeSide ? -MovementAxisWorld : MovementAxisWorld);
			CameraDirection = FVector::VectorPlaneProject(CameraDirection, FVector::UpVector).GetSafeNormal();
			if (CameraDirection.IsNearlyZero())
			{
				CameraDirection =
					(bCutRemovedNegativeSide ? -MovementAxisWorld : MovementAxisWorld).GetSafeNormal();
			}

			// 水平偏航扰动：每片在 ±CutPieceYawJitterDeg 内朝不同方向飞，避免规整
			if (CutPieceYawJitterDeg > 0.0f)
			{
				// 用切片中心 + 时间作种子，保证确定性但仍每片不同
				const uint32 Seed = static_cast<uint32>(
					FMath::Abs(OtherHalf->GetComponentLocation().X * 1000.0f)
					+ FMath::Abs(OtherHalf->GetComponentLocation().Y * 31.0f));
				FRandomStream Rng(Seed);
				const float JitterRad = FMath::DegreesToRadians(CutPieceYawJitterDeg);
				const float YawOffset = (Rng.FRand() * 2.0f - 1.0f) * JitterRad;
				const FQuat YawRot(FVector::UpVector, YawOffset);
				CameraDirection = YawRot.RotateVector(CameraDirection).GetSafeNormal();
			}

			// ---- 速度大小：基础 + 体积系数（大块更猛，小片更轻） ----
			// CutVolume 在上方已算（CutAwayTotal * VoxelVol），用其做尺寸代理。
			const float EffVolume = FMath::Min(CutVolume, CutPieceVolumeSpeedCap);
			const float SpeedMag = CutPieceBaseSpeed + CutPieceVolumeSpeedK * EffVolume;

			// 水平推力（速度增量，bVelChange=true 与质量无关）
			const FVector HorizontalVel = CameraDirection * SpeedMag;

			// 垂直上抛：让切片腾空翻一下再落地，不是纯平推
			const FVector LiftVel(0.0f, 0.0f, CutPieceLiftSpeed);

			OtherHalf->AddImpulse(HorizontalVel + LiftVel, NAME_None, true);

			// ---- 倾倒角速度：方向轴也加随机化，翻飞感 ----
			FVector TipAxis = FVector::CrossProduct(FVector::UpVector, CameraDirection).GetSafeNormal();
			{
				const uint32 ASeed = static_cast<uint32>(
					FMath::Abs(OtherHalf->GetComponentLocation().Z * 1000.0f)
					+ FMath::Abs(OtherHalf->GetComponentLocation().X * 7.0f));
				FRandomStream ARng(ASeed);
				const float ARad = FMath::DegreesToRadians(25.0f) * ARng.FRand();
				const FQuat ARot(CameraDirection, ARad);
				TipAxis = ARot.RotateVector(TipAxis).GetSafeNormal();
			}
			OtherHalf->AddAngularImpulseInRadians(
				TipAxis * CutPieceTipAngularSpeed, NAME_None, true);
		}
		else
		{
			UE_LOG(LogClaudeCore, Error,
				TEXT("[ClcCuttingTable][CutPhys] OtherHalf has no valid local bounds"));
		}

		// 方案1: 每次切割前先清掉上一刀的碎块，防止连切两刀后第一刀碎块永不被销毁
		if (LastCutAwayPiece.IsValid())
		{
			LastCutAwayPiece->DestroyComponent();
			LastCutAwayPiece.Reset();
		}
		if (CutPieceCleanupHandle.IsValid() && GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(CutPieceCleanupHandle);
		}

		LastCutAwayPiece = OtherHalf;
		if (CutPieceCleanupDelay > 0.0f && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimer(
				CutPieceCleanupHandle, this, &AClcCuttingTable::CleanupLastCutPiece,
				CutPieceCleanupDelay, false);
		}
	}

	// ---- 后处理：耐久、预算结算、金币、飞金币、Toast ----
	Durability->SetDurability(EClcRepairableTool::Blade,
		CurrentDurability - BladeDurabilityPerCut);
	CuttingStone->GetStoneData(ActiveStoneData);

	int32 PieceGold = 0;
	int32 ConsumedBudgetAfter = ActiveStoneData.ConsumedCutBudget;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{

			PieceGold = Market->CalculateCutPieceValue(
				ActiveStoneData, CutAwayTotal, CutAwayJade,
				CutAwayCrack, CutAwayImpurity,
				VoxelVol, CuttingStone->GetTotalVoxels(),
				CuttingStone->GetLastCutJadeBoundingBox(),
				ConsumedBudgetAfter);
		}
	}

	// 无论金币是否为零，都写入 ConsumedCutBudget 和 TotalSettledValue
	CuttingStone->ApplyCutSettlement(ConsumedBudgetAfter, PieceGold);

	// 同步 ActiveStoneData（ApplyCutSettlement 已更新石头 Actor 本地副本）
	CuttingStone->GetStoneData(ActiveStoneData);

	if (PieceGold > 0 && CachedBackpack)
	{
		CachedBackpack->AddGold(PieceGold);

		if (CachedPC.IsValid())
		{
			const FVector WorldPos = CuttingStone->GetCutPieceWorldLocation();
			FVector2D ScreenPos;
			if (CachedPC->ProjectWorldLocationToScreen(WorldPos, ScreenPos, false))
			{
				UClcGoldFlyWidget* FlyWidget = CreateWidget<UClcGoldFlyWidget>(CachedPC.Get(), UClcGoldFlyWidget::StaticClass());
				if (FlyWidget)
				{
					FlyWidget->AddToViewport(120);
					FlyWidget->StartFlight(ScreenPos, PieceGold);
				}
			}
		}
	}

	OnBladeCut(CutAwayTotal, CutAwayJade, CutAwayCrack);
	PushHUDData();

	// 缓存切割结果，ShowCutFace 阶段飘字展示
	LastCutAwayTotal = CutAwayTotal;
	LastCutAwayJade = CutAwayJade;
	LastCutAwayCrack = CutAwayCrack;
	LastCutAwayImpurity = CutAwayImpurity;
	LastPieceGold = PieceGold;
	LastCutVolume = CutVolume;
}

void AClcCuttingTable::ShowCutFaceInfo()
{
	if (!CachedPC.IsValid()) return;

	const float JadeRatio = LastCutAwayTotal > 0
		? 100.0f * static_cast<float>(LastCutAwayJade) / static_cast<float>(LastCutAwayTotal)
		: 0.0f;
	const float CrackRatio = LastCutAwayTotal > 0
		? 100.0f * static_cast<float>(LastCutAwayCrack) / static_cast<float>(LastCutAwayTotal)
		: 0.0f;

	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		Toast->AddLog(FString::Printf(TEXT("切下 %.0f cm³  + %d 金"), LastCutVolume, LastPieceGold),
			ShowCutFaceDuration, FLinearColor(1.0f, 0.9f, 0.4f));
		Toast->AddLog(FString::Printf(TEXT("玉 %.0f%% | 裂 %.0f%% | 杂 %.0f%%"),
			JadeRatio, CrackRatio, 100.0f - JadeRatio - CrackRatio),
			ShowCutFaceDuration, FLinearColor(0.3f, 0.9f, 0.5f));
	}
}

void AClcCuttingTable::FinishCutCinematic()
{
	if (LeftCutCamera) LeftCutCamera->SetActive(false);
	if (RightCutCamera) RightCutCamera->SetActive(false);
	if (WorkCamera) WorkCamera->SetActive(true);
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(this, 0.3f);
	}

	CurrentState = EClcCuttingTableState::StoneOnBench;
	BladePhase = EBladePhase::Idle;
	PhaseTimer = 0.0f;
	bCutExecutedThisCycle = false;
	ActiveCutCamera.Reset();
	bCutKeyPrev = true; // 防止玩家一直按住 SpaceBar 立即再触发
}

void AClcCuttingTable::CleanupLastCutPiece()
{
	if (CutPieceCleanupHandle.IsValid() && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CutPieceCleanupHandle);
	}
	if (UProceduralMeshComponent* Piece = LastCutAwayPiece.Get())
	{
		Piece->DestroyComponent();
	}
	LastCutAwayPiece.Reset();
}

bool AClcCuttingTable::PlaceStoneOnBench(int32 StoneIndex)
{
	if (!CachedBackpack || !StoneSpawnPoint) return false;

	const TArray<FClcStoneRuntimeData> Stones = CachedBackpack->GetStones();
	if (!Stones.IsValidIndex(StoneIndex)) return false;
	if (!IsStoneEligible(Stones[StoneIndex])) return false;

	ActiveStoneData = Stones[StoneIndex];
	if (!CachedBackpack->RemoveStone(StoneIndex))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CuttingStone = GetWorld()->SpawnActor<AClcCuttingStone>(
		AClcCuttingStone::StaticClass(), StoneSpawnPoint->GetComponentLocation(),
		StoneSpawnPoint->GetComponentRotation(), SpawnParams);

	auto Rollback = [this]()
	{
		DestroyCuttingStone();
		if (CachedBackpack && CachedBackpack->AddStone(ActiveStoneData) < 0)
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcCuttingTable] Failed to return stone after placement error."));
		}
		ActiveStoneData = FClcStoneRuntimeData();
	};

	if (!CuttingStone)
	{
		Rollback();
		return false;
	}

	CuttingStone->AttachToComponent(StoneSpawnPoint, FAttachmentTransformRules::KeepWorldTransform);
	const float TargetCoverage = ResolveTargetCoverage(ActiveStoneData);
	const int32 DefectCount = FMath::Clamp(2 + FMath::RoundToInt(TargetCoverage * 4.0f), 2, 5);
	if (!CuttingStone->Initialize(ActiveStoneData, DefectCount, TargetCoverage,
		VoxelResolution, ShellMaterialPath))
	{
		Rollback();
		return false;
	}

	CuttingStone->AutoAlignLongestAxis();
	MovementAxisWorld = StoneSpawnPoint->GetForwardVector().GetSafeNormal();
	StoneBaseWorldLocation = StoneSpawnPoint->GetComponentLocation();
	StoneOffset = 0.0f;
	MovementRange = FMath::Max(0.0f,
		CuttingStone->GetHalfExtentAlongWorldAxis(MovementAxisWorld) - CutEdgeMargin);
	CuttingStone->SetActorLocation(StoneBaseWorldLocation);
	CuttingStone->GetStoneData(ActiveStoneData);

	CurrentState = EClcCuttingTableState::StoneOnBench;
	CreateHUD();
	PushHUDData();
	HUDPushTimer = HUDPushInterval;
	OnStonePlaced(ActiveStoneData.Internal);

	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		Toast->AddLog(FString::Printf(TEXT("上台：%s"), *ActiveStoneData.DisplayName),
			2.0f, FLinearColor::White);
	}
	return true;
}

void AClcCuttingTable::RemoveStoneFromBench()
{
	if (!CuttingStone) return;

	DestroyHUD();
	FClcStoneRuntimeData UpdatedData;
	if (CuttingStone->GetStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	if (CachedBackpack && CachedBackpack->AddStone(ActiveStoneData) < 0)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcCuttingTable] Failed to return active stone to backpack."));
	}

	DestroyCuttingStone();
	ActiveStoneData = FClcStoneRuntimeData();
	StoneOffset = 0.0f;
	MovementRange = 0.0f;
	CurrentState = EClcCuttingTableState::AwaitingStone;
	OnStoneRemoved();
}

void AClcCuttingTable::DestroyCuttingStone()
{
	if (AimDecal)
	{
		AimDecal->SetVisibility(false);
	}
	if (CuttingStone)
	{
		CuttingStone->Destroy();
		CuttingStone = nullptr;
	}
}

void AClcCuttingTable::OnBackpackStoneSelected(int32 StoneIndex)
{
	if (!CachedBackpack) return;
	const TArray<FClcStoneRuntimeData> Stones = CachedBackpack->GetStones();
	if (!Stones.IsValidIndex(StoneIndex)) return;

	const FClcStoneRuntimeData& Selected = Stones[StoneIndex];
	if (Selected.bHaggleResolved)
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("这块石头已锁价，不能上解石台"), 2.2f, FLinearColor(1.0f, 0.5f, 0.2f));
		}
		return;
	}
	if (Selected.Phase == EClcStonePhase::Windowed)
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("已擦石的原石不能再解石"), 2.2f, FLinearColor::Yellow);
		}
		return;
	}

	if (CurrentState == EClcCuttingTableState::StoneOnBench)
	{
		RemoveStoneFromBench();
	}

	if (CurrentState == EClcCuttingTableState::AwaitingStone && PlaceStoneOnBench(StoneIndex))
	{
		if (UClcBackpackSubsystem* Backpack = GetBackpack(); Backpack && Backpack->IsBackpackOpen())
		{
			Backpack->ToggleBackpack();
		}
		SetCuttingInputMode();
	}
}

void AClcCuttingTable::UnbindFromBackpackWidget()
{
	if (UClcBackpackSubsystem* Backpack = GetBackpack())
	{
		if (UClcBackpackWidget* Widget = Backpack->GetBackpackWidget())
		{
			Widget->OnStoneSelected.RemoveDynamic(this, &AClcCuttingTable::OnBackpackStoneSelected);
		}
	}
}

void AClcCuttingTable::SetCuttingInputMode()
{
	if (!CachedPC.IsValid()) return;
	CachedPC->bShowMouseCursor = false;
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(CachedPC.Get());
}

bool AClcCuttingTable::IsStoneEligible(const FClcStoneRuntimeData& StoneData) const
{
	return !StoneData.bHaggleResolved && StoneData.Phase != EClcStonePhase::Windowed;
}

bool AClcCuttingTable::HasEligibleStone() const
{
	if (!CachedBackpack) return false;
	for (const FClcStoneRuntimeData& Stone : CachedBackpack->GetStones())
	{
		if (IsStoneEligible(Stone)) return true;
	}
	return false;
}

float AClcCuttingTable::ResolveTargetCoverage(const FClcStoneRuntimeData& StoneData) const
{
	const float StoredCoverage = FMath::Max(StoneData.Internal.BlackRatio,
		StoneData.Internal.ImpurityRatio + StoneData.Internal.CrackRatio);
	return FMath::Clamp(StoredCoverage > KINDA_SMALL_NUMBER
		? StoredCoverage : FallbackDefectCoverage, 0.01f, 0.95f);
}

bool AClcCuttingTable::CanCutNow() const
{
	if (!CuttingStone || !BladePoint) return false;
	if (!CuttingStone->CanCutAtWorldPlane(BladePoint->GetComponentLocation(), MovementAxisWorld))
	{
		return false;
	}

	if (const UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld()))
	{
		return Durability->GetDurability(EClcRepairableTool::Blade) + KINDA_SMALL_NUMBER
			>= BladeDurabilityPerCut;
	}
	return false;
}

bool AClcCuttingTable::IsRemainingStoneInStandardRange() const
{
	if (!CuttingStone || CuttingStone->GetTotalVoxels() <= 0) return false;

	int32 RemainingTotal = 0, RemainingJade = 0, RemainingCrack = 0, RemainingImpurity = 0;
	CuttingStone->GetVoxelField().CountRemainingVoxels(RemainingTotal, RemainingJade, RemainingCrack, RemainingImpurity);
	if (RemainingTotal <= 0) return false;

	const float Ratio = static_cast<float>(RemainingTotal) / static_cast<float>(CuttingStone->GetTotalVoxels());

	float Threshold = 0.15f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			if (const UClcStoneConfig* Cfg = Market->GetStoneConfig())
			{
				Threshold = Cfg->QuickSellRatioThreshold;
			}
		}
	}

	return Ratio < Threshold;
}

bool AClcCuttingTable::SellRemainingStone()
{
	if (!CuttingStone || !CachedBackpack || !CachedPC.IsValid()) return false;

	FClcStoneRuntimeData StoneData;
	if (!CuttingStone->GetStoneData(StoneData)) return false;

	int32 SellPrice = 0;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			SellPrice = Market->CalculateCutStoneSalePrice(StoneData);
		}
	}

	if (SellPrice > 0)
	{
		CachedBackpack->AddGold(SellPrice);

		// 通知任务系统：卖出石头 +1
		if (CachedPC.IsValid())
		{
			if (const ULocalPlayer* LP = CachedPC->GetLocalPlayer())
			{
				if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
				{
					QS->NotifyObjectiveProgress(EClcQuestObjectiveType::SellStones, 1);
				}
			}
		}

		const FVector WorldPos = CuttingStone->GetCutPieceWorldLocation();
		FVector2D ScreenPos;
		if (CachedPC->ProjectWorldLocationToScreen(WorldPos, ScreenPos, false))
		{
			UClcGoldFlyWidget* FlyWidget = CreateWidget<UClcGoldFlyWidget>(CachedPC.Get(), UClcGoldFlyWidget::StaticClass());
			if (FlyWidget)
			{
				FlyWidget->AddToViewport(120);
				FlyWidget->StartFlight(ScreenPos, SellPrice);
			}
		}
	}

	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		Toast->AddLog(FString::Printf(TEXT("剩余主体售出 +%d 金"), SellPrice),
			2.5f, FLinearColor(0.0f, 1.0f, 0.8f));
	}

	// 出售后不归还石头到背包，直接销毁
	DestroyHUD();
	DestroyCuttingStone();
	ActiveStoneData = FClcStoneRuntimeData();
	StoneOffset = 0.0f;
	MovementRange = 0.0f;
	OnStoneRemoved();

	// 背包无可用石头 → 自动退出；有 → 自动打开背包选下一块
	if (HasEligibleStone())
	{
		CurrentState = EClcCuttingTableState::AwaitingStone;
		if (UClcBackpackSubsystem* Backpack = GetBackpack())
		{
			if (!Backpack->IsBackpackOpen())
			{
				Backpack->ToggleBackpack();
			}
			BindToBackpackWidget();
		}
	}
	else
	{
		// 先切到非 StoneOnBench 状态，避免 ExitCuttingMode 里 RemoveStoneFromBench 访问已销毁的 CuttingStone
		CurrentState = EClcCuttingTableState::AwaitingStone;
		ExitCuttingMode();
	}

	return true;
}

bool AClcCuttingTable::GetActiveStone(FClcStoneRuntimeData& OutData) const
{
	return CuttingStone && CuttingStone->GetStoneData(OutData);
}

void AClcCuttingTable::CreateHUD()
{
	DestroyHUD();
	if (!HUDWidgetClass || !CachedPC.IsValid()) return;

	HUDWidget = CreateWidget<UClcCuttingTableHUD>(CachedPC.Get(), HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(10);
		HUDPushTimer = 0.0f;
	}
}

void AClcCuttingTable::DestroyHUD()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
}

EClcCutSizeState AClcCuttingTable::ComputeCutSizeState(float& OutRatio) const
{
	OutRatio = 0.0f;
	if (!CanCutNow() || !BladePoint || !CuttingStone)
	{
		return EClcCutSizeState::CannotCut;
	}

	float Ratio = 0.0f;
	if (!CuttingStone->PredictCutRatio(BladePoint->GetComponentLocation(), MovementAxisWorld, Ratio))
	{
		return EClcCutSizeState::CannotCut;
	}

	OutRatio = Ratio;
	float MinR = 0.15f, MaxR = 0.45f;
	float FloorR = 0.05f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			if (const UClcStoneConfig* Cfg = Market->GetStoneConfig())
			{
				MinR   = Cfg->IdealCutRatioRange.X;
				MaxR   = Cfg->IdealCutRatioRange.Y;
				FloorR = Cfg->MinCutRatioForValue;
			}
		}
	}

	// 比例过低（低于 MinR 或低于硬地板比例）→ 黄色警告（实际金币为 0 或压缩）
	if (Ratio < MinR || Ratio < FloorR) return EClcCutSizeState::Undersized;
	if (Ratio > MaxR) return EClcCutSizeState::Oversized;
	return EClcCutSizeState::Standard;
}

void AClcCuttingTable::PushHUDData()
{
	if (!CuttingStone)
	{
		UpdateAimDecalColor(EClcCutSizeState::CannotCut);
		return;
	}

	float CutSizeRatio = 0.0f;
	const EClcCutSizeState CutSizeState = ComputeCutSizeState(CutSizeRatio);
	UpdateAimDecalColor(CutSizeState);

	if (!HUDWidget) return;

	FClcStoneRuntimeData StoneData;
	if (!CuttingStone->GetStoneData(StoneData)) return;

	FClcCuttingTableHUDData Data;
	Data.DisplayName = StoneData.DisplayName;
	Data.Origin = StoneData.Internal.Origin;
	Data.bGradeRevealed = StoneData.CutPlanes.Num() > 0;
	Data.GradeValue = static_cast<uint8>(StoneData.Internal.Grade);
	Data.CutCount = StoneData.CutPlanes.Num();
	{
		const float TotalVol = StoneData.ExposedCutVolume + StoneData.RemainingVolume;
		Data.CutProgress = TotalVol > KINDA_SMALL_NUMBER
			? StoneData.ExposedCutVolume / TotalVol : 0.0f;
	}
	Data.StoneOffset = StoneOffset;
	Data.MovementRange = MovementRange;
	Data.bCanCut = CanCutNow();
	Data.SettledGold = StoneData.TotalSettledValue;
	Data.PurchasePrice = StoneData.Internal.PurchasePrice;

	// ---- 切块尺寸预判四态（与 Decal 共用本次预测结果）----
	Data.CutSizeState = CutSizeState;
	Data.CutSizeRatio = CutSizeRatio;

	// ---- 剩余主体一键出售 ----
	Data.bCanSellRemaining = IsRemainingStoneInStandardRange();
	if (Data.bCanSellRemaining)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
			{
				Data.RemainingSellPrice = Market->CalculateCutStoneSalePrice(StoneData);
			}
		}
		Data.OperationHints = FString::Printf(
			TEXT("A / D 移动原石 | 空格 出售剩余主体 (+%d 金)\nB 更换原石 | Esc 退出"),
			Data.RemainingSellPrice);
	}
	else
	{
		Data.RemainingSellPrice = 0;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			Data.CurrentValuation = Market->CalculateSalePrice(StoneData);
		}
	}

	if (UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld()))
	{
		Data.BladeCurrent = Durability->GetDurability(EClcRepairableTool::Blade);
		Data.BladeMax = Durability->GetMaxDurability(EClcRepairableTool::Blade);
		Data.BladeDurability = Durability->GetDurabilityRatio(EClcRepairableTool::Blade);
	}

	HUDWidget->RefreshData(Data);
}

bool AClcCuttingTable::IsStoneSelectable(const FClcStoneRuntimeData& Stone) const
{
	return IsStoneEligible(Stone);
}

void AClcCuttingTable::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	PlayerInRange = Pawn;
	CachePlayerRefs();

	if (HasEligibleStone())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastEnterToastTime > 3.0)
		{
			LastEnterToastTime = Now;
			if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
			{
				Toast->AddLog(TEXT("按 F 使用解石台"), 2.0f, FLinearColor(0.0f, 1.0f, 1.0f));
			}
		}
	}
}

void AClcCuttingTable::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (PlayerInRange.Get() != Other) return;

	if (CurrentState != EClcCuttingTableState::Inactive)
	{
		ExitCuttingMode();
	}

	// F 键提示已由 UClcInteractionComponent 统一管理

	PlayerInRange.Reset();
	CachedPC.Reset();
	CachedBackpack = nullptr;
	CachedBackpack = nullptr;
}

void AClcCuttingTable::OnEnterCuttingMode_Implementation()
{
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(this, 0.3f);
		CachedPC->SetIgnoreMoveInput(true);
		CachedPC->SetIgnoreLookInput(true);
	}

	if (APawn* Pawn = PlayerInRange.Get())
	{
		Pawn->SetActorHiddenInGame(true);
		if (CachedPC.IsValid())
		{
			Pawn->DisableInput(CachedPC.Get());
		}
	}
}

void AClcCuttingTable::OnExitCuttingMode_Implementation()
{
	if (CachedPC.IsValid())
	{
		CachedPC->SetIgnoreMoveInput(false);
		CachedPC->SetIgnoreLookInput(false);
		if (APawn* Pawn = CachedPC->GetPawn())
		{
			Pawn->SetActorHiddenInGame(false);
			Pawn->EnableInput(CachedPC.Get());
			CachedPC->SetViewTargetWithBlend(Pawn, 0.2f);
		}
	}
}

void AClcCuttingTable::OnStonePlaced_Implementation(const FClcStoneInternalData& StoneData)
{
}

void AClcCuttingTable::OnStoneRemoved_Implementation()
{
}

void AClcCuttingTable::OnBladeCut_Implementation(int32 CutAwayTotal, int32 CutAwayJade,
	int32 CutAwayCrack)
{
}

// ============================================================
// 自适应补光
// ============================================================

void AClcCuttingTable::UpdateFillLightTarget()
{
	switch (CurrentState)
	{
	case EClcCuttingTableState::Inactive:
		TargetFillLightIntensity = FillLightInactiveIntensity;
		return;
	case EClcCuttingTableState::AwaitingStone:
		TargetFillLightIntensity = FillLightIdleIntensity;
		return;
	case EClcCuttingTableState::StoneOnBench:
		TargetFillLightIntensity = StoneOnBenchFillLightIntensity;
		return;
	case EClcCuttingTableState::CuttingCinematic:
		TargetFillLightIntensity = StoneOnBenchFillLightIntensity;
		return;
	}
}

bool AClcCuttingTable::UpdateAimDecalTransform()
{
	if (!AimDecal || !CuttingStone || !BladePoint) return false;

	UPrimitiveComponent* StoneMesh = CuttingStone->GetDisplayMesh();
	if (!StoneMesh) return false;

	const FBoxSphereBounds Bounds = StoneMesh->Bounds;
	if (Bounds.BoxExtent.IsNearlyZero()) return false;

	const FVector CutNormal = MovementAxisWorld.GetSafeNormal();
	if (CutNormal.IsNearlyZero()) return false;

	const FVector ProjectionAxis = FVector::UpVector;
	FVector LineDirection = FVector::CrossProduct(ProjectionAxis, CutNormal).GetSafeNormal();
	if (LineDirection.IsNearlyZero())
	{
		LineDirection = FVector::RightVector;
	}

	LineDirection = FQuat(
		ProjectionAxis,
		FMath::DegreesToRadians(AimDecalRotationOffset)).RotateVector(LineDirection).GetSafeNormal();

	// Decal 本地 X 指向表面外法线，实际沿 -X 投入表面；本地 Z 是修正后的主切线方向。
	const FRotator DecalRotation = FRotationMatrix::MakeFromXZ(
		ProjectionAxis, LineDirection).Rotator();

	FVector Location = Bounds.Origin;
	Location += CutNormal * FVector::DotProduct(
		BladePoint->GetComponentLocation() - Bounds.Origin, CutNormal);
	Location.Z = Bounds.Origin.Z + Bounds.BoxExtent.Z + AimDecalSurfaceOffset;

	const float ProjectionDepth = Bounds.BoxExtent.Z * 2.0f
		+ AimDecalSurfaceOffset + AimDecalProjectionPadding;
	const float LineHalfLength = Bounds.BoxExtent.GetMax() * AimDecalCoverageScale;
	const float LineHalfWidth = FMath::Max(Bounds.BoxExtent.GetMin() * 0.35f, 20.0f);

	AimDecal->SetWorldLocationAndRotation(Location, DecalRotation);
	AimDecal->DecalSize = FVector(ProjectionDepth, LineHalfWidth, LineHalfLength);
	AimDecal->MarkRenderStateDirty();
	return true;
}

void AClcCuttingTable::UpdateAimDecalColor(EClcCutSizeState State)
{
	if (!AimDecal) return;

	const bool bShouldShow = (CurrentState == EClcCuttingTableState::StoneOnBench)
		&& (BladePhase == EBladePhase::Idle) && CuttingStone != nullptr;
	if (!bShouldShow || !UpdateAimDecalTransform())
	{
		AimDecal->SetVisibility(false);
		return;
	}
	AimDecal->SetVisibility(true);

	// 懒创建 MID（材质未配置时静默跳过，不阻塞配色逻辑）
	if (!AimDecalMID && AimDecal)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *AimDecalMaterialPath))
		{
			AimDecal->SetDecalMaterial(Mat);
			AimDecalMID = AimDecal->CreateDynamicMaterialInstance();
		}
		else
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[AimDecal] FAILED to load material: %s"), *AimDecalMaterialPath);
		}
	}
	if (!AimDecalMID) return;

	AimDecalMID->SetScalarParameterValue(TEXT("LineWidth"), AimDecalLineWidth);

	const uint8 StateKey = static_cast<uint8>(State);
	if (StateKey == LastAimDecalState) return;
	LastAimDecalState = StateKey;

	FLinearColor Color;
	switch (State)
	{
	case EClcCutSizeState::CannotCut:
		Color = FLinearColor(1.0f, 0.1f, 0.05f, 0.8f); break;  // 红
	case EClcCutSizeState::Standard:
		Color = FLinearColor(0.2f, 1.0f, 0.2f, 0.95f); break; // 绿
	default: // Undersized / Oversized
		Color = FLinearColor(1.0f, 0.55f, 0.2f, 0.9f); break;  // 橙
	}
	AimDecalMID->SetVectorParameterValue(TEXT("Color"), Color);
	AimDecalMID->SetScalarParameterValue(TEXT("Opacity"), Color.A);
}
