// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStoneVendor.h"
#include "ClcLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "Components/ClcHaggleComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Actors/ClcOpeningStone.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcShellTextureConfig.h"
#include "UI/ClcBackpackWidget.h"
#include "UI/ClcVendorHUD.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

namespace
{
	UClcLogToastSubsystem* GetLogToast(const TWeakObjectPtr<APlayerController>& PC)
	{
		if (!PC.IsValid()) return nullptr;
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			return LP->GetSubsystem<UClcLogToastSubsystem>();
		}
		return nullptr;
	}
}

AClcStoneVendor::AClcStoneVendor()
{
	// 每帧 Tick：WasInputKeyJustPressed 的 flag 只保留一帧 + 补光平滑过渡
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

	// 无缩放根——避免 BP 缩放 VendorMesh 污染 TriggerSphere/相机
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

	// 石头生成定位点——上台石头挂这下面
	StoneSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnPoint"));
	StoneSpawnPoint->SetupAttachment(VendorRoot);
	StoneSpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));

	// 观察相机摇臂——挂 StoneSpawnPoint，跟随石头
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(StoneSpawnPoint);
	CameraArm->TargetArmLength = CameraDistance;
	CameraArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
	CameraArm->bDoCollisionTest = false;
	CameraArm->bUsePawnControlRotation = false;
	CameraArm->bInheritPitch = true;
	CameraArm->bInheritYaw = true;
	CameraArm->bInheritRoll = true;

	VendorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VendorCamera"));
	VendorCamera->SetupAttachment(CameraArm);

	// 自适应补光——位置/锥角/颜色在 BP 的 FillLight 组件上调，这里只给默认值
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

	// NPC 站位锚点（蓝图可拖动；箭头位置=站位，箭头朝向=面朝方向）
	NpcSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("NpcSpawnPoint"));
	NpcSpawnPoint->SetupAttachment(VendorRoot);
	NpcSpawnPoint->SetRelativeLocation(FVector(90.0f, 0.0f, 0.0f));

	// NPC 网格体——挂锚点，位置/朝向随箭头；默认空，运行时由 DA_HaggleConfig 装 Mesh
	NpcMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("NpcMesh"));
	NpcMesh->SetupAttachment(NpcSpawnPoint);
	NpcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NpcMesh->SetGenerateOverlapEvents(false);

	// 讨价还价 QTE 组件（ActorComponent，无附着）
	HaggleComponent = CreateDefaultSubobject<UClcHaggleComponent>(TEXT("HaggleComponent"));
}

void AClcStoneVendor::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->SetSphereRadius(InteractionRadius);

	if (InteractionIndicator)
	{
		InteractionIndicator->InteractionRadius = InteractionRadius;
		InteractionIndicator->bSelectByProximity = true;
		InteractionIndicator->OnQueryCanSelect.BindDynamic(this, &AClcStoneVendor::QueryCanSelect);
	}

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcStoneVendor::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcStoneVendor::OnTriggerEndOverlap);

	// 初始化 BaseFOV——避免首次进入时 aim zoom 基准错误
	if (VendorCamera)
	{
		BaseFOV = VendorCamera->FieldOfView;
	}

	// 绑定讨价还价组件回调
	if (HaggleComponent)
	{
		HaggleComponent->OnHaggleOpened.AddDynamic(this, &AClcStoneVendor::HandleHaggleOpened);
		HaggleComponent->OnHaggleResolved.AddDynamic(this, &AClcStoneVendor::HandleHaggleResolved);
	}

	// 用配置初始化 NPC 网格体 + 待机动画（资产来自 DA_HaggleConfig）
	SetupNpcFromConfig();
}

void AClcStoneVendor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 补光平滑过渡——放所有 early return 之前，保证进/出展示都能过渡
	TickFillLight(DeltaTime);

	// Inactive：检测进入按键
	if (CurrentState == EClcVendorState::Inactive && PlayerInRange.IsValid())
	{
		if (CachedPC.IsValid() && CachedPC->WasInputKeyJustPressed(EnterKey))
		{
			EnterSellMode();
			return;
		}
	}

	// 活跃态：Esc 退出（背包开 → 先关背包，不退出）
	if (CurrentState != EClcVendorState::Inactive && CachedPC.IsValid())
	{
		const bool bExitDown = CachedPC->IsInputKeyDown(ExitKey);
		if (bExitDown && !bExitKeyPrev)
		{
			bExitKeyPrev = true;
			if (CurrentState == EClcVendorState::Haggling)
			{
				// 讨价还价 Esc：选档→取消回查看；QTE（未失败）→回退重选；结算中→忽略
				if (HaggleComponent) HaggleComponent->RequestEsc();
			}
			else if (CachedBackpack && CachedBackpack->IsBackpackOpen())
			{
				CachedBackpack->ToggleBackpack();
			}
			else
			{
				ExitSellMode();
				return;
			}
		}
		else if (!bExitDown)
		{
			bExitKeyPrev = false;
		}
	}

	// StoneOnBench：旋转/复位/放大/售出 + HUD 定时推送
	if (CurrentState == EClcVendorState::StoneOnBench)
	{
		ProcessStoneOnBenchInput(DeltaTime);

		HUDPushTimer -= DeltaTime;
		if (HUDPushTimer <= 0.0f)
		{
			PushVendorHUDData();
			HUDPushTimer = HUDPushInterval;
		}
	}
}

void AClcStoneVendor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 销毁前清理——退出模式 + 子 Actor + Widget + 委托，避免悬空
	if (CurrentState != EClcVendorState::Inactive)
	{
		ExitSellMode();
	}
	DestroyOpeningStone();
	DestroyVendorHUD();

	if (InteractionIndicator)
	{
		InteractionIndicator->OnQueryCanSelect.Unbind();
	}

	TriggerSphere->OnComponentBeginOverlap.RemoveDynamic(this, &AClcStoneVendor::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.RemoveDynamic(this, &AClcStoneVendor::OnTriggerEndOverlap);

	// 兜底注销按键提示
	if (VendorPromptHandle != 0 && CachedPC.IsValid())
	{
		if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
			{
				KP->UnregisterKeyPrompt(VendorPromptHandle);
			}
		}
		VendorPromptHandle = 0;
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
	// 接口契约预留（让 GetAllActorsWithInterface 收集到本 Actor）；
	// 实际进入仍由自身 Tick 轮询 F 键，与工作台一致。被调用时若已激活则忽略。
	if (CurrentState != EClcVendorState::Inactive) return false;
	EnterSellMode();
	return true;
}

// ============================================================
// 售出（键盘 Enter 与 HUD 按钮共用入口）
// ============================================================

void AClcStoneVendor::RequestSell()
{
	if (CurrentState != EClcVendorState::StoneOnBench || !OpeningStone) return;

	FClcStoneRuntimeData StoneRT;
	const bool bHave = OpeningStone->GetStoneData(StoneRT);

	// 已讨价锁定 → 直接按锁价售出（不再讨价）
	if (bHave && StoneRT.bHaggleResolved)
	{
		CompleteSellWithPrice(StoneRT.HaggleLockedPrice);
		return;
	}

	// 未启用讨价还价 → 直接按参考价售出（旧行为）
	if (!bEnableHaggle || !HaggleComponent || !CachedPC.IsValid())
	{
		CompleteSell();
		return;
	}

	// 进讨价还价：用当前参考价
	int32 RefPrice = 0;
	if (bHave)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
			{
				RefPrice = Market->CalculateSalePrice(StoneRT);
			}
		}
	}

	CurrentState = EClcVendorState::Haggling;
	HaggleComponent->StartHaggle(RefPrice, CachedPC.Get());
}

void AClcStoneVendor::CompleteSell()
{
	if (CurrentState != EClcVendorState::StoneOnBench || !OpeningStone) return;

	UClcStoneMarketSubsystem* Market = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Market = GI->GetSubsystem<UClcStoneMarketSubsystem>();
		}
	}
	if (!Market)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcVendor] MarketSubsystem unavailable!"));
		return;
	}

	// 用台上石当前数据算参考价，再走统一售出
	FClcStoneRuntimeData StoneRT;
	OpeningStone->GetStoneData(StoneRT);
	CompleteSellWithPrice(Market->CalculateSalePrice(StoneRT));
}

void AClcStoneVendor::CompleteSellWithPrice(int32 Price)
{
	if (!OpeningStone) return;

	// 读最新数据（vendor 不开窗，数据未变，保持链路一致）
	FClcStoneRuntimeData UpdatedData;
	if (OpeningStone->GetStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	// 加金。注意：石头在 PlaceStoneOnVendor 时已从背包移除，这里【绝不】二次 RemoveStone。
	if (CachedBackpack)
	{
		CachedBackpack->AddGold(Price);
	}

	// 销毁台上石头
	DestroyOpeningStone();

	// 通知蓝图（音效/特效/动画）
	OnStoneSold(ActiveStoneData, Price);

	if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
	{
		TArray<FStringFormatArg> SoldArgs;
		SoldArgs.Add(FStringFormatArg(ActiveStoneData.DisplayName));
		SoldArgs.Add(FStringFormatArg(Price));
		LT->AddLog(FString::Format(*SoldTip.ToString(), SoldArgs), 2.0f, FLinearColor::Green);
	}

	ActiveStoneData = FClcStoneRuntimeData();

	// 背包空 → 自动退出；有货 → 回选石（开背包选下一块）
	if (CachedBackpack && CachedBackpack->GetStones().Num() == 0)
	{
		if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
		{
			LT->AddLog(SoldEmptyExitTip.ToString(), 2.0f, FLinearColor::Yellow);
		}
		ExitSellMode();
	}
	else
	{
		CurrentState = EClcVendorState::AwaitingStone;

		if (CachedBackpack && !CachedBackpack->IsBackpackOpen())
		{
			CachedBackpack->ToggleBackpack();
		}
		BindToBackpackWidget();

		// 刷一帧 awaiting 态 HUD（bCanSell=false）
		PushVendorHUDData();

		if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
		{
			LT->AddLog(SoldHasMoreTip.ToString(), 2.0f, FLinearColor(0.f, 1.f, 1.f));
		}
	}
}

// ============================================================
// 进入 / 退出展示
// ============================================================

void AClcStoneVendor::CachePlayerRefs()
{
	if (APawn* Pawn = PlayerInRange.Get())
	{
		CachedPC = Cast<APlayerController>(Pawn->GetController());
	}

	if (CachedPC.IsValid())
	{
		if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
		{
			CachedBackpack = LP->GetSubsystem<UClcBackpackSubsystem>();
		}
	}
}

void AClcStoneVendor::EnterSellMode()
{
	if (!CachedPC.IsValid() || !CachedBackpack) return;

	// 背包空 → 拒绝
	if (CachedBackpack->GetStones().Num() == 0)
	{
		if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
		{
			LT->AddLog(EmptyTip.ToString(), 2.0f, FLinearColor::Yellow);
		}
		return;
	}

	CurrentState = EClcVendorState::AwaitingStone;

	// 开窗模式隐藏小白点（切相机避免碍事）
	if (InteractionIndicator) InteractionIndicator->bHidden = true;

	OnEnterSellMode(); // 切相机 + 锁输入（_Implementation）

	// 打开背包
	if (!CachedBackpack->IsBackpackOpen())
	{
		CachedBackpack->ToggleBackpack();
	}
	BindToBackpackWidget();

	// 活跃态持续显示光标：兼顾 WASD 旋转 + HUD 售出按钮点击
	SetVendorCursor(true);

	if (VendorCamera) BaseFOV = VendorCamera->FieldOfView;

	// 出 HUD（会话期复用，换石/售出下一块不重建）
	CreateVendorHUD();
	PushVendorHUDData();

	if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
	{
		LT->AddLog(EnterTip.ToString(), 2.0f, FLinearColor(0.f, 1.f, 1.f));
	}
}

void AClcStoneVendor::ExitSellMode()
{
	// 关闭背包
	if (CachedBackpack && CachedBackpack->IsBackpackOpen())
	{
		CachedBackpack->ToggleBackpack();
	}

	// 解绑选石委托
	if (CachedBackpack)
	{
		if (UClcBackpackWidget* Widget = CachedBackpack->GetBackpackWidget())
		{
			Widget->OnStoneSelected.RemoveDynamic(this, &AClcStoneVendor::OnBackpackStoneSelected);
		}
	}

	// 台上有石 → 放回背包
	if (CurrentState == EClcVendorState::StoneOnBench)
	{
		RemoveStoneFromVendor();
	}

	// 恢复 FOV（右键放大可能改过）
	if (VendorCamera) VendorCamera->SetFieldOfView(BaseFOV);

	CurrentState = EClcVendorState::Inactive;

	DestroyVendorHUD();

	// 恢复小白点（玩家还在范围内会自动显示）
	if (InteractionIndicator) InteractionIndicator->bHidden = false;

	OnExitSellMode(); // 恢复相机 + 输入（_Implementation）

	SetVendorCursor(false);

	if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
	{
		LT->AddLog(ExitTip.ToString(), 2.0f, FLinearColor::White);
	}
}

// ============================================================
// 石头放置 / 回收 / 销毁
// ============================================================

void AClcStoneVendor::PlaceStoneOnVendor(int32 StoneIndex)
{
	if (!CachedBackpack) return;

	TArray<FClcStoneRuntimeData> AllStones = CachedBackpack->GetStones();
	if (!AllStones.IsValidIndex(StoneIndex))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] Invalid stone index: %d"), StoneIndex);
		return;
	}

	// 取出石头数据
	ActiveStoneData = AllStones[StoneIndex];

	// 从背包移除（保持索引一致；石头此刻仅存于 OpeningStone + ActiveStoneData）
	CachedBackpack->RemoveStone(StoneIndex);

	// Spawn AClcOpeningStone（不挂任何工具，纯展示已开窗状态）
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
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] Failed to spawn OpeningStone!"));
		return;
	}

	// 挂到 StoneSpawnPoint，让石头跟随定位点
	OpeningStone->AttachToComponent(StoneSpawnPoint, FAttachmentTransformRules::KeepWorldTransform);

	// 初始化石头（加载 Mesh + 材质 + 还原已开窗遮罩）
	if (!OpeningStone->Initialize(ActiveStoneData, OpeningMaterialPath))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] OpeningStone Initialize failed!"));
		OpeningStone->Destroy();
		OpeningStone = nullptr;
		return;
	}

	// 背包开闭状态监听初始化（B 键开关由全局 IA_Backpack 处理，这里只轮询响应）
	bBackpackWasOpen = CachedBackpack->IsBackpackOpen();

	CurrentState = EClcVendorState::StoneOnBench;

	// 立即刷一帧 HUD
	PushVendorHUDData();
	HUDPushTimer = HUDPushInterval;

	if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
	{
		TArray<FStringFormatArg> PlacedArgs;
		PlacedArgs.Add(FStringFormatArg(ActiveStoneData.DisplayName));
		LT->AddLog(FString::Format(*PlacedTip.ToString(), PlacedArgs), 2.0f, FLinearColor::White);
	}

	// 锁价石上台：额外提示玩家直接出手
	if (ActiveStoneData.bHaggleResolved)
	{
		if (UClcLogToastSubsystem* LT2 = GetLogToast(CachedPC))
		{
			LT2->AddLog(TEXT("【锁价石】Enter 直接出手，不可再讨价"), 2.5f, FLinearColor(1.0f, 0.85f, 0.2f));
		}
	}
}

void AClcStoneVendor::RemoveStoneFromVendor()
{
	if (!OpeningStone) return;

	// 读最新数据
	FClcStoneRuntimeData UpdatedData;
	if (OpeningStone->GetStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	// 放回背包
	if (CachedBackpack)
	{
		CachedBackpack->AddStone(ActiveStoneData);
	}

	DestroyOpeningStone();

	ActiveStoneData = FClcStoneRuntimeData();
}

void AClcStoneVendor::DestroyOpeningStone()
{
	if (OpeningStone)
	{
		OpeningStone->Destroy();
		OpeningStone = nullptr;
	}
}

// ============================================================
// 输入处理——WASD 旋转 + R 复位 + 右键放大 + Enter 售出
// ============================================================

void AClcStoneVendor::ProcessStoneOnBenchInput(float DeltaTime)
{
	if (!CachedPC.IsValid() || !OpeningStone) return;

	// 右键 FOV 放大（纯视觉拉近）
	UpdateAimZoom(DeltaTime);

	// WASD 旋转（相机相对）
	const float RotAmount = OpeningStone->GetRotationSpeed() * DeltaTime * RotationInputScale;

	float DeltaPitch = 0.0f;
	float DeltaYaw = 0.0f;

	if (CachedPC->IsInputKeyDown(EKeys::W)) DeltaPitch -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::S)) DeltaPitch += RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::A)) DeltaYaw -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::D)) DeltaYaw += RotAmount;

	if (!FMath::IsNearlyZero(DeltaPitch) || !FMath::IsNearlyZero(DeltaYaw))
	{
		bResetRotationPending = false; // 用户手动旋转 → 取消复位
		OpeningStone->AddRotationInput(DeltaPitch, DeltaYaw,
			VendorCamera->GetRightVector(), VendorCamera->GetUpVector());
	}

	// R 键旋转复位
	{
		const bool bRDown = CachedPC->IsInputKeyDown(ResetRotationKey);
		if (bRDown && !bRKeyPrev)
		{
			bRKeyPrev = true;
			bResetRotationPending = true;
		}
		else if (!bRDown)
		{
			bRKeyPrev = false;
		}
	}

	if (bResetRotationPending && OpeningStone)
	{
		if (OpeningStone->ResetRotation(DeltaTime, ResetRotationSpeed))
		{
			bResetRotationPending = false;
		}
	}

	// 背包开闭轮询（B 键开关由全局 IA_Backpack 处理，这里响应变化 → 重新绑委托支持换石）
	if (CachedBackpack)
	{
		const bool bNowOpen = CachedBackpack->IsBackpackOpen();
		if (bNowOpen != bBackpackWasOpen)
		{
			if (bNowOpen)
			{
				BindToBackpackWidget();
			}
			bBackpackWasOpen = bNowOpen;
		}
	}

	// Enter 售出（边沿检测，防连按；放最后，售出后 OpeningStone 已销毁）
	{
		const bool bSellDown = CachedPC->IsInputKeyDown(SellKey);
		if (bSellDown && !bSellKeyPrev)
		{
			bSellKeyPrev = true;
			RequestSell();
		}
		else if (!bSellDown)
		{
			bSellKeyPrev = false;
		}
	}
}

void AClcStoneVendor::UpdateAimZoom(float DeltaTime)
{
	if (!VendorCamera) return;

	const bool bAimDown = CachedPC.IsValid() && CachedPC->IsInputKeyDown(EKeys::RightMouseButton);
	const float TargetFOV = bAimDown ? (BaseFOV / AimZoomFactor) : BaseFOV;
	const float CurrentFOV = VendorCamera->FieldOfView;
	const float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, AimZoomSpeed);
	VendorCamera->SetFieldOfView(NewFOV);
}

// ============================================================
// 背包交互
// ============================================================

void AClcStoneVendor::BindToBackpackWidget()
{
	if (!CachedBackpack) return;

	UClcBackpackWidget* Widget = CachedBackpack->GetBackpackWidget();
	if (!Widget) return;

	Widget->OnStoneSelected.RemoveDynamic(this, &AClcStoneVendor::OnBackpackStoneSelected);
	Widget->OnStoneSelected.AddDynamic(this, &AClcStoneVendor::OnBackpackStoneSelected);
}

void AClcStoneVendor::OnBackpackStoneSelected(int32 StoneIndex)
{
	// AwaitingStone：首次选石 → 上台
	if (CurrentState == EClcVendorState::AwaitingStone)
	{
		PlaceStoneOnVendor(StoneIndex);

		// 关背包（展示模式靠 WASD + 售出按钮，不需要背包）
		if (CachedBackpack && CachedBackpack->IsBackpackOpen())
		{
			CachedBackpack->ToggleBackpack();
		}
	}
	// StoneOnBench：换石——先关背包 + 放回当前 + 上新
	else if (CurrentState == EClcVendorState::StoneOnBench)
	{
		if (CachedBackpack && CachedBackpack->IsBackpackOpen())
		{
			CachedBackpack->ToggleBackpack();
		}

		if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
		{
			TArray<FStringFormatArg> SwapArgs;
			SwapArgs.Add(FStringFormatArg(ActiveStoneData.DisplayName));
			LT->AddLog(FString::Format(*SwapTip.ToString(), SwapArgs), 2.0f, FLinearColor::White);
		}

		RemoveStoneFromVendor();
		PlaceStoneOnVendor(StoneIndex);
	}
}

// ============================================================
// 光标
// ============================================================

void AClcStoneVendor::SetVendorCursor(bool bVisible)
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
// 自适应补光
// ============================================================

void AClcStoneVendor::UpdateFillLightTarget()
{
	TargetFillLightIntensity = (CurrentState == EClcVendorState::Inactive)
		? FillLightInactiveIntensity
		: FillLightDisplayIntensity;
}

void AClcStoneVendor::TickFillLight(float DeltaTime)
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
	// 强度趋近 0 时关掉组件，省光照开销
	FillLight->SetVisibility(CurrentFillLightIntensity > KINDA_SMALL_NUMBER);
}

// ============================================================
// HUD
// ============================================================

void AClcStoneVendor::CreateVendorHUD()
{
	DestroyVendorHUD();

	if (!CachedPC.IsValid()) return;

	TSubclassOf<UClcVendorHUD> WidgetClass = HUDWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UClcVendorHUD::StaticClass();
	}

	HUDWidget = CreateWidget<UClcVendorHUD>(CachedPC.Get(), WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->OwningVendor = this;
		HUDWidget->AddToViewport(10);
		HUDPushTimer = 0.0f; // 立即推送
	}
}

void AClcStoneVendor::DestroyVendorHUD()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
}

void AClcStoneVendor::PushVendorHUDData()
{
	if (!HUDWidget) return;

	FClcVendorHUDData Data;

	if (OpeningStone)
	{
		FClcStoneRuntimeData StoneRT;
		if (OpeningStone->GetStoneData(StoneRT))
		{
			const auto& I = StoneRT.Internal;
			Data.DisplayName   = StoneRT.DisplayName;
			Data.Origin        = I.Origin;
			Data.GradeValue    = (uint8)I.Grade;
			Data.PurchasePrice = I.PurchasePrice;

			// 种水是否暴露：vendor 不开窗，OpenedGreenArea>0 即已暴露
			Data.bGradeRevealed = StoneRT.OpenedGreenArea > 0.0f;

			// 皮壳名称
			Data.ShellName = UClcShellTextureConfig::GetShellName(I.ShellTypeIndex).ToString();

			// 开窗进度
			float OpenedR, GreenR, BlackR, ImpurityR, CrackR;
			OpeningStone->GetOpeningProgress(OpenedR, GreenR, BlackR, ImpurityR, CrackR);
			Data.OpenedRatio = OpenedR;
			Data.SurfaceArea = I.SurfaceArea;
			Data.GreenArea   = GreenR * I.SurfaceArea;
			Data.BlackArea   = BlackR * I.SurfaceArea;

			// 回收价 + 盈亏：已讨价锁定用锁价，否则实时算
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
				{
					const int32 SalePrice = StoneRT.bHaggleResolved
						? StoneRT.HaggleLockedPrice
						: Market->CalculateSalePrice(StoneRT);
					Data.SalePrice = SalePrice;
					Data.ValuationTrend = SalePrice > I.PurchasePrice ? 1 : (SalePrice < I.PurchasePrice ? -1 : 0);
					Data.ProfitAmount = SalePrice - I.PurchasePrice;
				}
			}

			// 锁价石：HUD 操作提示显式标注已锁价、Enter 直接出手（不可再讨）
			if (StoneRT.bHaggleResolved)
			{
				Data.OperationHints = TEXT("【已锁价】Enter 直接出手 · 不可再讨价\nWASD 旋转 | R 复位 | 右键 放大 | B 换石 | Esc 退出");
			}
		}

		Data.bCanSell = true;
	}
	else
	{
		// awaiting 态：无台上石，售出按钮禁用
		Data.bCanSell = false;
	}

	// 钱包余额 & 背包状态
	if (CachedBackpack)
	{
		Data.GoldBalance = CachedBackpack->GetGold();
		Data.bBackpackOpen = CachedBackpack->IsBackpackOpen();
	}

	HUDWidget->RefreshData(Data);
}

// ============================================================
// 触发器
// ============================================================

void AClcStoneVendor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(Other))
	{
		if (Pawn->IsLocallyControlled())
		{
			PlayerInRange = Pawn;
			CachePlayerRefs();

			if (VendorPromptHandle == 0 && CachedPC.IsValid())
			{
				if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
				{
					if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
					{
						VendorPromptHandle = KP->RegisterKeyPrompt(
							EnterKey,
							NSLOCTEXT("ClcVendor", "VendorPromptLabel", "出售石头"),
							FName("Vendor"), 100);
					}
				}
			}
		}
	}
}

void AClcStoneVendor::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (PlayerInRange.Get() == Other)
	{
		PlayerInRange.Reset();

		if (VendorPromptHandle != 0 && CachedPC.IsValid())
		{
			if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
			{
				if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
				{
					KP->UnregisterKeyPrompt(VendorPromptHandle);
				}
			}
			VendorPromptHandle = 0;
		}

		// 变更点：离开范围自动退出展示（与工作台一致，作安全网；
		// 新交互已 SetIgnoreMoveInput，玩家正常走不出范围，仅在异常脱离时兜底）。
		if (CurrentState != EClcVendorState::Inactive)
		{
			ExitSellMode();
		}
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
	// 切相机到回收台 + 锁玩家移动/视角
	if (CachedPC.IsValid())
	{
		CachedPC->SetViewTargetWithBlend(this, 0.3f);
		CachedPC->SetIgnoreMoveInput(true);
		CachedPC->SetIgnoreLookInput(true);

		// 暂时隐藏玩家角色，避免其站位阻挡回收台相机/产生碰撞
		if (APawn* MyPawn = CachedPC->GetPawn())
		{
			MyPawn->SetActorHiddenInGame(true);
		}
	}
}

void AClcStoneVendor::OnExitSellMode_Implementation()
{
	// 恢复玩家相机 + 输入
	if (CachedPC.IsValid())
	{
		// 先恢复玩家角色显示，再切回相机，避免淡入时角色还没出来
		if (APawn* MyPawn = CachedPC->GetPawn())
		{
			MyPawn->SetActorHiddenInGame(false);
		}
		CachedPC->SetViewTargetWithBlend(CachedPC->GetPawn(), 0.2f);
		CachedPC->SetIgnoreMoveInput(false);
		CachedPC->SetIgnoreLookInput(false);
	}
}

// ============================================================
// 讨价还价回调（HaggleComponent 多播驱动）
// ============================================================

void AClcStoneVendor::HandleHaggleOpened()
{
	OnNpcMakeOffer(); // 蓝图覆写播 NPC 报价演绎
}

void AClcStoneVendor::HandleHaggleResolved(EClcHaggleOutcome Outcome, int32 FinalPrice, float AppliedRatio)
{
	(void)AppliedRatio;

	switch (Outcome)
	{
	case EClcHaggleOutcome::Cancelled:
		OnNpcHaggleCancel();
		// 回到查看态，石头留在台上不售出（未锁定，可再讨价）
		CurrentState = EClcVendorState::StoneOnBench;
		break;

	case EClcHaggleOutcome::Accepted:
		// 「直接出手」→ 立即按参考价售出，走独立演绎
		OnNpcHaggleAccept(FinalPrice);
		CurrentState = EClcVendorState::StoneOnBench;
		CompleteSellWithPrice(FinalPrice);
		break;

	case EClcHaggleOutcome::Success:
		OnNpcHaggleSuccess(FinalPrice);
		LockHagglePriceAndReturn(FinalPrice); // 锁价待玩家手动售出
		break;

	case EClcHaggleOutcome::Failure:
		OnNpcHaggleFail(FinalPrice);
		LockHagglePriceAndReturn(FinalPrice);
		break;
	}
}

void AClcStoneVendor::LockHagglePriceAndReturn(int32 LockedPrice)
{
	// 锁定价写到石头运行时数据（之后售出/开窗都据此门禁）
	if (OpeningStone)
	{
		OpeningStone->MarkHaggleResolved(LockedPrice);
	}

	CurrentState = EClcVendorState::StoneOnBench;

	// 立即刷新 HUD 显示锁价
	PushVendorHUDData();

	if (UClcLogToastSubsystem* LT = GetLogToast(CachedPC))
	{
		TArray<FStringFormatArg> Args;
		Args.Add(FStringFormatArg(LockedPrice));
		LT->AddLog(FString::Format(*HaggleLockedTip.ToString(), Args), 2.5f, FLinearColor(1.0f, 0.85f, 0.2f));
	}
}

// ============================================================
// NPC 演绎（C++ 自动播放，资产来自 HaggleConfig；BP 仍可覆写加音效/特效）
// ============================================================

void AClcStoneVendor::SetupNpcFromConfig()
{
	if (!HaggleComponent || !NpcMesh) return;

	UClcHaggleConfig* Cfg = HaggleComponent->GetHaggleConfig();
	if (!Cfg || !Cfg->NpcSkeletalMesh) return; // 无网格体 → NpcMesh 保持空（纯 UI 讨价还价）

	NpcMesh->SetSkeletalMesh(Cfg->NpcSkeletalMesh);
	if (Cfg->NpcIdleAnim)
	{
		NpcMesh->PlayAnimation(Cfg->NpcIdleAnim, true);
	}

	// 装好网格体后把脚贴到地面
	SnapNpcToGround();
}

void AClcStoneVendor::SnapNpcToGround()
{
	if (!NpcSpawnPoint || !NpcMesh) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 从锚点稍上方向下 trace 找地面（忽略 vendor 自身）
	const FVector Anchor = NpcSpawnPoint->GetComponentLocation();
	const FVector Start = Anchor + FVector(0.0f, 0.0f, 50.0f);
	const FVector End = Anchor - FVector(0.0f, 0.0f, GroundSnapTraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
	{
		// NpcMesh 原点（=脚）放到地面命中点的 Z，XY 取锚点；朝向仍随箭头
		NpcMesh->SetWorldLocation(FVector(Anchor.X, Anchor.Y, Hit.Location.Z));
	}
}

void AClcStoneVendor::PlayNpcAnim(UAnimSequence* Anim, bool bLoop)
{
	if (!NpcMesh || !Anim) return;

	NpcMesh->PlayAnimation(Anim, bLoop);

	if (!bLoop)
	{
		// 非循环状态动画播完自动回 Idle
		GetWorldTimerManager().ClearTimer(NpcReturnIdleTimer);
		const float Len = Anim->GetPlayLength();
		if (Len > 0.0f)
		{
			GetWorldTimerManager().SetTimer(NpcReturnIdleTimer, this, &AClcStoneVendor::ReturnToNpcIdle, Len + 0.05f, false);
		}
	}
}

void AClcStoneVendor::ReturnToNpcIdle()
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcIdleAnim, true);
	}
}

void AClcStoneVendor::OnNpcMakeOffer_Implementation()
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcOfferAnim, false);
	}
}

void AClcStoneVendor::OnNpcHaggleSuccess_Implementation(int32 FinalPrice)
{
	(void)FinalPrice;
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcSuccessAnim, false);
	}
}

void AClcStoneVendor::OnNpcHaggleAccept_Implementation(int32 FinalPrice)
{
	(void)FinalPrice;
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcAcceptAnim, false);
	}
}

void AClcStoneVendor::OnNpcHaggleFail_Implementation(int32 FinalPrice)
{
	(void)FinalPrice;
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcFailureAnim, false);
	}
}

void AClcStoneVendor::OnNpcHaggleCancel_Implementation()
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcCancelAnim, false);
	}
}
