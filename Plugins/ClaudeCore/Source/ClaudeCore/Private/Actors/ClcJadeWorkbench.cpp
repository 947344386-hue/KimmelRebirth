// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcJadeWorkbench.h"
#include "Actors/ClcOpeningStone.h"
#include "Tools/ClcStoneTool.h"
#include "Tools/ClcOpeningTool.h"
#include "Tools/ClcFlashlightTool.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcShellTextureConfig.h"
#include "UI/ClcBackpackWidget.h"
#include "UI/ClcWorkbenchHUD.h"
#include "Interfaces/ClcStoneCarrier.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

AClcJadeWorkbench::AClcJadeWorkbench()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;

	// ---- 根组件 ----
	BenchRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BenchRoot"));
	RootComponent = BenchRoot;

	// ---- 触发器 ----
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(BenchRoot);
	TriggerSphere->InitSphereRadius(TriggerRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// ---- 桌面 Mesh ----
	BenchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BenchMesh"));
	BenchMesh->SetupAttachment(BenchRoot);
	BenchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// ---- 石头生成定位点 ----
	StoneSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnPoint"));
	StoneSpawnPoint->SetupAttachment(BenchRoot);
	StoneSpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	// ---- 观察摄像机 ----
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(StoneSpawnPoint);
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	CameraArm->bDoCollisionTest = false;
	CameraArm->bUsePawnControlRotation = false;
	CameraArm->bInheritPitch = false;
	CameraArm->bInheritYaw = false;
	CameraArm->bInheritRoll = false;

	WorkCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("WorkCamera"));
	WorkCamera->SetupAttachment(CameraArm);

	// ---- 工具类默认值（用户可在 Workbench Details 中改为 BP 子类） ----
	OpeningToolClass = AClcOpeningTool::StaticClass();
	FlashlightToolClass = AClcFlashlightTool::StaticClass();
}

void AClcJadeWorkbench::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcJadeWorkbench::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcJadeWorkbench::OnTriggerEndOverlap);

	TriggerSphere->SetSphereRadius(TriggerRadius);
}

void AClcJadeWorkbench::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Inactive 状态：检测进入按键
	if (CurrentState == EClcWorkbenchState::Inactive && PlayerInRange.IsValid())
	{
		if (CachedPC.IsValid() && CachedPC->WasInputKeyJustPressed(EnterKey))
		{
			EnterOpeningMode();
			return;
		}
	}

	// 开窗模式：检测退出——背包打开时优先关背包
	if (CurrentState != EClcWorkbenchState::Inactive && CachedPC.IsValid())
	{
		const bool bExitDown = CachedPC->IsInputKeyDown(ExitKey);
		if (bExitDown && !bExitKeyPrev)
		{
			bExitKeyPrev = true;
			// 背包开着 → 关背包，不退出工作台
			UClcBackpackSubsystem* BP = CachedCarrierObj.IsValid()
				? Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get())
				: nullptr;
			if (BP && BP->IsBackpackOpen())
			{
				BP->ToggleBackpack();
				SetWorkbenchCursor(true);
			}
			else
			{
				ExitOpeningMode();
				return;
			}
		}
		else if (!bExitDown)
		{
			bExitKeyPrev = false;
		}
	}

	// StoneOnBench 状态：处理 WASD 旋转 + 工具操作 + 背包 + HUD
	if (CurrentState == EClcWorkbenchState::StoneOnBench)
	{
		ProcessStoneOnBenchInput(DeltaTime);

		// HUD 定时推送
		HUDPushTimer -= DeltaTime;
		if (HUDPushTimer <= 0.0f)
		{
			PushHUDData();
			HUDPushTimer = HUDPushInterval;
		}
	}
}

// ============================================================
// 输入处理——WASD 旋转 + 工具（鼠标追踪 + 左键）+ T 切换 + B 键切石
// ============================================================

void AClcJadeWorkbench::ProcessStoneOnBenchInput(float DeltaTime)
{
	if (!CachedPC.IsValid() || !OpeningStone) return;

	// ---- 右键长按 FOV 放大（独立于工具，纯视觉拉近，不碰开窗/手电筒） ----
	UpdateAimZoom(DeltaTime);

	// ---- WASD 旋转 ----
	const float RotAmount = OpeningStone->GetRotationSpeed() * DeltaTime * RotationInputScale;

	float DeltaPitch = 0.0f;
	float DeltaYaw = 0.0f;

	if (CachedPC->IsInputKeyDown(EKeys::W)) DeltaPitch -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::S)) DeltaPitch += RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::A)) DeltaYaw -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::D)) DeltaYaw += RotAmount;

	if (!FMath::IsNearlyZero(DeltaPitch) || !FMath::IsNearlyZero(DeltaYaw))
	{
		OpeningStone->AddRotationInput(DeltaPitch, DeltaYaw);
	}

	// ---- T 键循环切换工具 ----
	{
		const bool bTDown = CachedPC->IsInputKeyDown(ToolSwitchKey);
		if (bTDown && !bTKeyPrev)
		{
			bTKeyPrev = true;
			CycleToolMode();
		}
		else if (!bTDown)
		{
			bTKeyPrev = false;
		}
	}

	// ---- -/= 键调整开窗笔刷半径 ----
	if (AClcOpeningTool* OpeningTool = Cast<AClcOpeningTool>(CurrentTool))
	{
		const bool bMinusDown = CachedPC->IsInputKeyDown(EKeys::Hyphen);
		const bool bEqualsDown = CachedPC->IsInputKeyDown(EKeys::Equals);
		// 连续按住式调整：每帧按比例叠加，短按也能触发，长按持续变化
		const float BrushSpeed = 5.0f; // 每秒约 5 次 BrushIncrementPerPress 的量
		if (bMinusDown)
		{
			OpeningTool->AdjustBrushRadius(-OpeningTool->BrushIncrementPerPress * BrushSpeed * DeltaTime);
		}
		if (bEqualsDown)
		{
			OpeningTool->AdjustBrushRadius(OpeningTool->BrushIncrementPerPress * BrushSpeed * DeltaTime);
		}
	}

	// ---- 背包打开时跳过工具操作 ----
	bool bBackpackOpen = false;
	if (CachedCarrierObj.IsValid())
	{
		if (auto BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get()))
		{
			bBackpackOpen = BP->IsBackpackOpen();
		}
	}

	if (!bBackpackOpen && CurrentTool)
	{
		// ---- 鼠标 LineTrace → 构建 TraceInfo ----
		FClcToolTraceInfo TraceInfo;

		FVector2D MousePos;
		if (CachedPC->GetMousePosition(MousePos.X, MousePos.Y))
		{
			FVector RayOrigin, RayDir;
			if (CachedPC->DeprojectScreenPositionToWorld(MousePos.X, MousePos.Y, RayOrigin, RayDir))
			{
				TraceInfo.RayOrigin = RayOrigin;
				TraceInfo.RayDirection = RayDir;

				FHitResult HitResult;
				const FVector RayEnd = RayOrigin + RayDir * 10000.0f;
				FCollisionQueryParams QueryParams;
				QueryParams.bTraceComplex = true;

				if (GetWorld()->LineTraceSingleByChannel(
					HitResult, RayOrigin, RayEnd,
					OpeningStone->GetTraceChannel(), QueryParams)
					&& HitResult.GetComponent() == OpeningStone->GetStoneMesh())
				{
					TraceInfo.bHasHit = true;
					TraceInfo.HitPoint = HitResult.ImpactPoint;
					TraceInfo.SurfaceNormal = HitResult.Normal;
				}
			}
		}

		// ---- 派发给工具 ----
		if (TraceInfo.bHasHit)
		{
			CurrentTool->OnUpdate(TraceInfo);
		}
		else
		{
			CurrentTool->OnUpdateIdle();
		}

		// ---- 左键边沿检测 ----
		bool bLeftDown = CachedPC->IsInputKeyDown(EKeys::LeftMouseButton);
		if (bLeftDown != bLeftMousePrev)
		{
			CurrentTool->OnLeftClick(bLeftDown);
			bLeftMousePrev = bLeftDown;
		}
	}

	// ---- 背包开闭状态轮询（B 键开关由全局 IA_Backpack 处理，这里只响应变化） ----
	if (CachedCarrierObj.IsValid())
	{
		if (auto BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get()))
		{
			const bool bNowOpen = BP->IsBackpackOpen();
			if (bNowOpen != bBackpackWasOpen)
			{
				if (bNowOpen)
				{
					// 背包刚开——绑定选石委托，用户可在背包里换石头
					BindToBackpackWidget();
				}
				else
				{
					// 背包刚关——开窗模式需要光标做打磨
					SetWorkbenchCursor(true);
				}
				bBackpackWasOpen = bNowOpen;
			}
		}
	}
}

// ============================================================
// 右键 FOV 放大
// ============================================================

void AClcJadeWorkbench::UpdateAimZoom(float DeltaTime)
{
	if (!WorkCamera) return;

	const bool bAimDown = CachedPC.IsValid() && CachedPC->IsInputKeyDown(EKeys::RightMouseButton);
	bIsAiming = bAimDown;

	const float TargetFOV = bAimDown ? (BaseFOV / AimZoomFactor) : BaseFOV;
	const float CurrentFOV = WorkCamera->FieldOfView;
	const float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, AimZoomSpeed);
	WorkCamera->SetFieldOfView(NewFOV);
}

// ============================================================
// 工具管理
// ============================================================

void AClcJadeWorkbench::CycleToolMode()
{
	EClcToolMode NewMode = (CurrentToolMode == EClcToolMode::Opener)
		? EClcToolMode::Flashlight
		: EClcToolMode::Opener;
	SwitchToolMode(NewMode);
}

void AClcJadeWorkbench::SwitchToolMode(EClcToolMode NewMode)
{
	if (CurrentToolMode == NewMode) return;

	DestroyCurrentTool();

	CurrentToolMode = NewMode;

	SpawnCurrentTool();

	OnToolModeChanged(NewMode);

	UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Tool mode → %s"),
		NewMode == EClcToolMode::Opener ? TEXT("Opener") : TEXT("Flashlight"));
}

void AClcJadeWorkbench::SpawnCurrentTool()
{
	if (!OpeningStone) return;

	TSubclassOf<AClcStoneTool> ToolClass;
	switch (CurrentToolMode)
	{
	case EClcToolMode::Opener:
		ToolClass = OpeningToolClass;
		break;
	case EClcToolMode::Flashlight:
		ToolClass = FlashlightToolClass;
		break;
	}

	if (!ToolClass) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	CurrentTool = GetWorld()->SpawnActor<AClcStoneTool>(
		ToolClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

	if (CurrentTool)
	{
		CurrentTool->Initialize(OpeningStone);
		CurrentTool->OnActivated();
	}
}

void AClcJadeWorkbench::DestroyCurrentTool()
{
	if (CurrentTool)
	{
		CurrentTool->OnDeactivated();
		CurrentTool->Destroy();
		CurrentTool = nullptr;
	}
}

// ============================================================
// 蓝图事件默认实现
// ============================================================

void AClcJadeWorkbench::OnToolModeChanged_Implementation([[maybe_unused]] EClcToolMode NewMode)
{
	// 蓝图覆写——刷新 UI、播放音效等
}

// ============================================================
// 触发器
// ============================================================

void AClcJadeWorkbench::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(Other))
	{
		if (Pawn->IsLocallyControlled())
		{
			PlayerInRange = Pawn;
			CachePlayerRefs();
			ShowPrompt(InteractionPrompt);
		}
	}
}

void AClcJadeWorkbench::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (PlayerInRange.Get() == Other)
	{
		PlayerInRange.Reset();
		HidePrompt();

		if (CurrentState != EClcWorkbenchState::Inactive)
		{
			ExitOpeningMode();
		}
	}
}

// ============================================================
// 进入 / 退出
// ============================================================

void AClcJadeWorkbench::CachePlayerRefs()
{
	if (APawn* Pawn = PlayerInRange.Get())
	{
		CachedPC = Cast<APlayerController>(Pawn->GetController());
	}

	if (CachedPC.IsValid())
	{
		if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				CachedCarrierObj = BP;
				CachedCarrier = static_cast<IClcStoneCarrier*>(BP);
			}
		}
	}
}

void AClcJadeWorkbench::EnterOpeningMode()
{
	if (!CachedPC.IsValid() || !CachedCarrier) return;

	if (CachedCarrier->GetStones().Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcWorkbench] Player has no stones."));
		return;
	}

	CurrentState = EClcWorkbenchState::AwaitingStone;
	HidePrompt();
	OnEnterOpeningMode();

	// 打开背包
	UClcBackpackSubsystem* BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get());
	if (BP && !BP->IsBackpackOpen())
	{
		BP->ToggleBackpack();
	}
	BindToBackpackWidget();

	// 缓存基础 FOV，右键放大基于此值缩放
	if (WorkCamera) BaseFOV = WorkCamera->FieldOfView;

	UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Entered opening mode."));
}

void AClcJadeWorkbench::ExitOpeningMode()
{
	// 关闭背包
	UClcBackpackSubsystem* BP = CachedCarrierObj.IsValid()
		? Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get())
		: nullptr;
	if (BP && BP->IsBackpackOpen())
	{
		BP->ToggleBackpack();
	}

	// 解绑委托
	if (BP)
	{
		if (UClcBackpackWidget* Widget = BP->GetBackpackWidget())
		{
			Widget->OnStoneSelected.RemoveDynamic(this, &AClcJadeWorkbench::OnBackpackStoneSelected);
		}
	}

	// 回收石头（存档 + 销毁）
	if (CurrentState == EClcWorkbenchState::StoneOnBench)
	{
		RemoveStoneFromBench();
	}

	// 恢复 FOV（右键放大可能改过）
	if (WorkCamera) WorkCamera->SetFieldOfView(BaseFOV);
	bIsAiming = false;

	// 恢复摄像机
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(CachedPC->GetPawn(), 0.2f);
	}

	CurrentState = EClcWorkbenchState::Inactive;
	ActiveStoneBackpackIndex = -1;

	OnExitOpeningMode();

	// 恢复光标和输入模式
	SetWorkbenchCursor(false);

	if (PlayerInRange.IsValid())
	{
		ShowPrompt(InteractionPrompt);
	}

	UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Exited opening mode."));
}

// ============================================================
// 光标管理
// ============================================================

void AClcJadeWorkbench::SetWorkbenchCursor(bool bVisible)
{
	if (!CachedPC.IsValid()) return;

	if (bVisible)
	{
		CachedPC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		CachedPC->SetInputMode(InputMode);
	}
	else
	{
		CachedPC->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(CachedPC.Get());
	}
}

// ============================================================
// 背包交互
// ============================================================

void AClcJadeWorkbench::BindToBackpackWidget()
{
	if (!CachedCarrierObj.IsValid()) return;

	UClcBackpackSubsystem* BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get());
	if (!BP) return;

	UClcBackpackWidget* Widget = BP->GetBackpackWidget();
	if (!Widget) return;

	Widget->OnStoneSelected.RemoveDynamic(this, &AClcJadeWorkbench::OnBackpackStoneSelected);
	Widget->OnStoneSelected.AddDynamic(this, &AClcJadeWorkbench::OnBackpackStoneSelected);
}

void AClcJadeWorkbench::OnBackpackStoneSelected(int32 StoneIndex)
{
	// AwaitingStone 状态：首次选石
	if (CurrentState == EClcWorkbenchState::AwaitingStone)
	{
		PlaceStoneOnBench(StoneIndex);

		// 关闭背包，恢复打磨用光标
		UClcBackpackSubsystem* BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get());
		if (BP && BP->IsBackpackOpen())
		{
			BP->ToggleBackpack();
		}
		SetWorkbenchCursor(true);
	}
	// StoneOnBench 状态：切换石头——先存档回收当前石头，再放新石头
	else if (CurrentState == EClcWorkbenchState::StoneOnBench)
	{
		// 先关闭背包
		UClcBackpackSubsystem* BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get());
		if (BP && BP->IsBackpackOpen())
		{
			BP->ToggleBackpack();
		}

		// 存档回收当前石头
		RemoveStoneFromBench();

		// 放新石头
		PlaceStoneOnBench(StoneIndex);

		SetWorkbenchCursor(true);

		UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Switched stone on bench."));
	}
}

// ============================================================
// 石头放置 / 回收 / 销毁
// ============================================================

void AClcJadeWorkbench::PlaceStoneOnBench(int32 StoneIndex)
{
	if (!CachedCarrier) return;

	TArray<FClcStoneRuntimeData> AllStones = CachedCarrier->GetStones();
	if (!AllStones.IsValidIndex(StoneIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcWorkbench] Invalid stone index: %d"), StoneIndex);
		return;
	}

	// 取出石头数据
	ActiveStoneData = AllStones[StoneIndex];
	ActiveStoneBackpackIndex = StoneIndex;

	// 从背包移除（保持索引一致）
	if (CachedCarrierObj.IsValid())
	{
		if (UClcBackpackSubsystem* BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get()))
		{
			BP->RemoveStone(StoneIndex);
		}
	}

	// Spawn AClcOpeningStone
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	const FTransform SpawnTransform = StoneSpawnPoint->GetComponentTransform();

	OpeningStone = GetWorld()->SpawnActor<AClcOpeningStone>(
		AClcOpeningStone::StaticClass(),
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		SpawnParams);

	if (!OpeningStone)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcWorkbench] Failed to spawn OpeningStone!"));
		return;
	}

	// 初始化石头（加载 Mesh + 材质 + 遮罩）
	if (!OpeningStone->Initialize(ActiveStoneData, OpeningMaterialPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcWorkbench] OpeningStone Initialize failed!"));
		OpeningStone->Destroy();
		OpeningStone = nullptr;
		return;
	}

	// 生成默认工具（开窗器）
	CurrentToolMode = EClcToolMode::Opener;
	SpawnCurrentTool();

	// 默认工具没走 SwitchToolMode，这里手动触发一次让 BP 刷新客户端表现
	OnToolModeChanged(CurrentToolMode);

	// 创建 HUD 并立即推送首帧数据（不等 Timer）
	CreateHUD();
	bGradeRevealed = false;
	PushHUDData();
	HUDPushTimer = HUDPushInterval;

	// 初始化背包状态监听——B 键开关背包交给全局 IA_Backpack，这里只轮询响应
	if (CachedCarrierObj.IsValid())
	{
		if (auto BP = Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get()))
		{
			bBackpackWasOpen = BP->IsBackpackOpen();
		}
	}

	CurrentState = EClcWorkbenchState::StoneOnBench;

	// 蓝图通知
	OnStonePlaced(ActiveStoneData.Internal);

	UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Stone '%s' placed. Green:%.2f Black:%.2f Grade:%d"),
		*ActiveStoneData.DisplayName, ActiveStoneData.Internal.GreenRatio,
		ActiveStoneData.Internal.BlackRatio, (int32)ActiveStoneData.Internal.Grade);
}

void AClcJadeWorkbench::RemoveStoneFromBench()
{
	if (!OpeningStone) return;

	// 先销毁 HUD 和工具（避免访问即将被销毁的石头）
	DestroyHUD();
	DestroyCurrentTool();

	// 从 OpeningStone 读取最新开窗进度
	FClcStoneRuntimeData UpdatedData;
	if (OpeningStone->GetStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	// 放回背包
	if (CachedCarrier)
	{
		CachedCarrier->AddStone(ActiveStoneData);
	}

	// 销毁 Actor
	DestroyOpeningStone();

	ActiveStoneBackpackIndex = -1;
	ActiveStoneData = FClcStoneRuntimeData();

	OnStoneRemoved();

	UE_LOG(LogTemp, Log, TEXT("[ClcWorkbench] Stone recycled to backpack."));
}

void AClcJadeWorkbench::DestroyOpeningStone()
{
	if (OpeningStone)
	{
		OpeningStone->Destroy();
		OpeningStone = nullptr;
	}
}

// ============================================================
// 查询
// ============================================================

bool AClcJadeWorkbench::GetActiveStone(FClcStoneRuntimeData& OutData) const
{
	if (CurrentState == EClcWorkbenchState::StoneOnBench && OpeningStone)
	{
		return OpeningStone->GetStoneData(OutData);
	}
	return false;
}

// ============================================================
// 蓝图原生事件默认实现
// ============================================================

void AClcJadeWorkbench::OnEnterOpeningMode_Implementation()
{
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(this, 0.3f);
	}

	if (PlayerInRange.IsValid())
	{
		if (APlayerController* PC = Cast<APlayerController>(PlayerInRange->GetController()))
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}
}

void AClcJadeWorkbench::OnExitOpeningMode_Implementation()
{
	if (CachedPC.IsValid())
	{
		CachedPC->SetIgnoreMoveInput(false);
		CachedPC->SetIgnoreLookInput(false);
	}
}

void AClcJadeWorkbench::OnStonePlaced_Implementation(const FClcStoneInternalData& StoneData)
{
	// 蓝图覆写——播放音效、刷新 UI 等
}

void AClcJadeWorkbench::OnStoneRemoved_Implementation()
{
	// 蓝图覆写——播放音效等
}

void AClcJadeWorkbench::ShowPrompt_Implementation(const FText& PromptText)
{
	// 蓝图覆写——显示交互提示
}

void AClcJadeWorkbench::HidePrompt_Implementation()
{
	// 蓝图覆写——隐藏交互提示
}

// ============================================================
// HUD
// ============================================================

void AClcJadeWorkbench::CreateHUD()
{
	DestroyHUD();

	if (!HUDWidgetClass || !CachedPC.IsValid()) return;

	HUDWidget = CreateWidget<UClcWorkbenchHUD>(CachedPC.Get(), HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(10);
		HUDPushTimer = 0.0f; // 立即推送
	}
}

void AClcJadeWorkbench::DestroyHUD()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
}

void AClcJadeWorkbench::PushHUDData()
{
	if (!HUDWidget || !OpeningStone) return;

	FClcWorkbenchHUDData Data;

	// ── 石头信息 ──
	FClcStoneRuntimeData StoneRT;
	if (OpeningStone->GetStoneData(StoneRT))
	{
		const auto& I = StoneRT.Internal;
		Data.DisplayName    = StoneRT.DisplayName;
		Data.Origin         = I.Origin;
		Data.GradeValue     = (uint8)I.Grade;
		Data.PurchasePrice  = I.PurchasePrice;

		// 种水暴露状态——GrindAtUV 触发 dirty flag → 第一个 tick 同步
		if (OpeningStone->ConsumeHUDDirty())
		{
			bGradeRevealed = true;
		}
		Data.bGradeRevealed = bGradeRevealed;

		// 皮壳名称
		Data.ShellName = UClcShellTextureConfig::GetShellName(I.ShellTypeIndex).ToString();

		// 开窗进度
		float OpenedR, GreenR, BlackR;
		OpeningStone->GetOpeningProgress(OpenedR, GreenR, BlackR);
		Data.OpenedRatio = OpenedR;
		Data.SurfaceArea = I.SurfaceArea;
		Data.GreenArea   = GreenR * I.SurfaceArea;
		Data.BlackArea   = BlackR * I.SurfaceArea;

		// 回收估值
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
			{
				int32 SalePrice = Market->CalculateSalePrice(StoneRT);
				Data.CurrentValuation = SalePrice;
				Data.ValuationTrend = SalePrice > I.PurchasePrice ? 1 : (SalePrice < I.PurchasePrice ? -1 : 0);
			}
		}
	}

	// ── 工具 ──
	if (CurrentTool)
	{
		Data.ToolDurability = CurrentTool->GetDurabilityRatio();

		// 工具名——根据模式给中文
		Data.ToolName = (CurrentToolMode == EClcToolMode::Flashlight) ? TEXT("手电筒") : TEXT("开窗器");

		// 手电筒模式下查询灯是否开启
		if (AClcFlashlightTool* FT = Cast<AClcFlashlightTool>(CurrentTool))
		{
			Data.bToolActive = FT->IsLightOn();
		}
	}
	else
	{
		Data.ToolDurability = 0.0f;
	}

	HUDWidget->RefreshData(Data);
}

// ============================================================
