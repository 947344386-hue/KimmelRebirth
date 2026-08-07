// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcCuttingTable.h"
#include "Actors/ClcCuttingStone.h"
#include "ClcLog.h"
#include "Components/ClcInteractionIndicator.h"
#include "Components/ClcInteractionComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/ClcStoneCarrier.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
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

void AClcCuttingTable::CachePlayerRefs()
{
	if (APawn* Pawn = PlayerInRange.Get())
	{
		CachedPC = Cast<APlayerController>(Pawn->GetController());
	}

	if (!CachedPC.IsValid()) return;
	if (ULocalPlayer* LocalPlayer = CachedPC->GetLocalPlayer())
	{
		if (UClcBackpackSubsystem* Backpack = LocalPlayer->GetSubsystem<UClcBackpackSubsystem>())
		{
			CachedCarrierObj = Backpack;
			CachedCarrier = static_cast<IClcStoneCarrier*>(Backpack);
		}
	}
}

void AClcCuttingTable::EnterCuttingMode()
{
	if (!CachedPC.IsValid() || !CachedCarrier || CurrentState != EClcCuttingTableState::Inactive)
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
			const bool bEmpty = CachedCarrier->GetStones().IsEmpty();
			Toast->AddLog(bEmpty ? TEXT("背包空，无可解石的石头")
				: TEXT("背包中没有可解石的石头"), 2.2f, FLinearColor::Yellow);
		}
		return;
	}

	CurrentState = EClcCuttingTableState::AwaitingStone;
	InteractionIndicator->bHidden = true;
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
		ExecuteBladeDrop();
	}
	bCutKeyPrev = bCutDown;
}

bool AClcCuttingTable::ExecuteBladeDrop()
{
	if (!CuttingStone || !CachedPC.IsValid()) return false;

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

	UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld());
	if (!Durability) return false;
	Durability->InitTool(EClcRepairableTool::Blade, BladeMaxDurability);

	const float CurrentDurability = Durability->GetDurability(EClcRepairableTool::Blade);
	if (CurrentDurability + KINDA_SMALL_NUMBER < BladeDurabilityPerCut)
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("解石刀耐久不足，需前往修理站修复"), 2.5f, FLinearColor::Red);
		}
		return false;
	}

	const FVector PlanePoint = BladePoint->GetComponentLocation();
	if (!CuttingStone->CanCutAtWorldPlane(PlanePoint, MovementAxisWorld))
	{
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(TEXT("刀口未穿过剩余主体，请调整石位"), 1.8f, FLinearColor::Yellow);
		}
		return false;
	}

	int32 CutAwayTotal = 0;
	int32 CutAwayJade = 0;
	int32 CutAwayCrack = 0;
	if (!CuttingStone->ExecuteCut(PlanePoint, MovementAxisWorld, false,
		CutAwayTotal, CutAwayJade, CutAwayCrack))
	{
		return false;
	}

	Durability->SetDurability(EClcRepairableTool::Blade,
		CurrentDurability - BladeDurabilityPerCut);
	CuttingStone->GetStoneData(ActiveStoneData);

	// 切块折金币（体积驱动定价）
	const float VoxelVol = CuttingStone->GetVoxelField().VoxelVolume;
	int32 PieceGold = 0;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			PieceGold = Market->CalculateCutPieceValue(
				CutAwayTotal, CutAwayJade, CutAwayCrack,
				VoxelVol, ActiveStoneData.Internal.Grade);
		}
	}
	if (PieceGold > 0 && CachedCarrier)
	{
		CachedCarrier->AddGold(PieceGold);
		ActiveStoneData.TotalSettledValue += PieceGold;

		// 飞金币动效
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

	const float CutVolume = static_cast<float>(CutAwayTotal) * CuttingStone->GetVoxelField().VoxelVolume;
	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		if (PieceGold > 0)
		{
			Toast->AddLog(FString::Printf(TEXT("解下一块：%.0f cm³  +%d 金"), CutVolume, PieceGold),
				1.8f, FLinearColor(0.3f, 1.0f, 0.5f));
		}
		else
		{
			Toast->AddLog(FString::Printf(TEXT("解下一块：%.0f cm³（无价值）"), CutVolume),
				1.8f, FLinearColor(0.6f, 0.6f, 0.6f));
		}
	}
	return true;
}

bool AClcCuttingTable::PlaceStoneOnBench(int32 StoneIndex)
{
	if (!CachedCarrier || !StoneSpawnPoint) return false;

	const TArray<FClcStoneRuntimeData> Stones = CachedCarrier->GetStones();
	if (!Stones.IsValidIndex(StoneIndex)) return false;
	if (!IsStoneEligible(Stones[StoneIndex])) return false;

	ActiveStoneData = Stones[StoneIndex];
	if (!CachedCarrier->RemoveStone(StoneIndex))
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
		if (CachedCarrier && CachedCarrier->AddStone(ActiveStoneData) < 0)
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

	if (CachedCarrier && CachedCarrier->AddStone(ActiveStoneData) < 0)
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
	if (CuttingStone)
	{
		CuttingStone->Destroy();
		CuttingStone = nullptr;
	}
}

void AClcCuttingTable::OnBackpackStoneSelected(int32 StoneIndex)
{
	if (!CachedCarrier) return;
	const TArray<FClcStoneRuntimeData> Stones = CachedCarrier->GetStones();
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

void AClcCuttingTable::BindToBackpackWidget()
{
	if (UClcBackpackSubsystem* Backpack = GetBackpack())
	{
		if (UClcBackpackWidget* Widget = Backpack->GetBackpackWidget())
		{
			Widget->OnStoneSelected.RemoveDynamic(this, &AClcCuttingTable::OnBackpackStoneSelected);
			Widget->OnStoneSelected.AddDynamic(this, &AClcCuttingTable::OnBackpackStoneSelected);
		}
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
	if (!CachedCarrier) return false;
	for (const FClcStoneRuntimeData& Stone : CachedCarrier->GetStones())
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

void AClcCuttingTable::PushHUDData()
{
	if (!HUDWidget || !CuttingStone) return;

	FClcStoneRuntimeData StoneData;
	if (!CuttingStone->GetStoneData(StoneData)) return;

	FClcCuttingTableHUDData Data;
	Data.DisplayName = StoneData.DisplayName;
	Data.Origin = StoneData.Internal.Origin;
	Data.bGradeRevealed = StoneData.CutPlanes.Num() > 0;
	Data.GradeValue = static_cast<uint8>(StoneData.Internal.Grade);
	Data.CutCount = StoneData.CutPlanes.Num();
	Data.RemovedVolume = StoneData.ExposedCutVolume;
	Data.RemainingVolume = StoneData.RemainingVolume;
	Data.StoneOffset = StoneOffset;
	Data.MovementRange = MovementRange;
	Data.bCanCut = CanCutNow();
	Data.SettledGold = StoneData.TotalSettledValue;

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

UClcBackpackSubsystem* AClcCuttingTable::GetBackpack() const
{
	return CachedCarrierObj.IsValid() ? Cast<UClcBackpackSubsystem>(CachedCarrierObj.Get()) : nullptr;
}

bool AClcCuttingTable::QueryCanSelect()
{
	APlayerController* PlayerController = CachedPC.IsValid()
		? CachedPC.Get() : UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PlayerController || !PlayerController->GetLocalPlayer()) return false;

	if (UClcBackpackSubsystem* Backpack =
		PlayerController->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>())
	{
		for (const FClcStoneRuntimeData& Stone : Backpack->GetStones())
		{
			if (IsStoneEligible(Stone)) return true;
		}
	}
	return false;
}

bool AClcCuttingTable::IsLookedAtByPlayer() const
{
	// F 键路由已迁移到 UClcInteractionComponent::HandleInteractInput
	return false;
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
	CachedCarrierObj.Reset();
	CachedCarrier = nullptr;
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
			bPawnInputDisabled = true;
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
			if (bPawnInputDisabled)
			{
				Pawn->EnableInput(CachedPC.Get());
			}
			CachedPC->SetViewTargetWithBlend(Pawn, 0.2f);
		}
	}
	bPawnInputDisabled = false;
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
	}
}

void AClcCuttingTable::TickFillLight(float DeltaTime)
{
	if (!FillLight) return;

	UpdateFillLightTarget();

	if (FillLightTransitionSpeed <= 0.0f || DeltaTime <= 0.0f)
	{
		CurrentFillLightIntensity = TargetFillLightIntensity;
	}
	else
	{
		CurrentFillLightIntensity = FMath::FInterpTo(
			CurrentFillLightIntensity, TargetFillLightIntensity, DeltaTime, FillLightTransitionSpeed);
	}

	FillLight->SetIntensity(CurrentFillLightIntensity);
	FillLight->SetVisibility(CurrentFillLightIntensity > KINDA_SMALL_NUMBER);
}
