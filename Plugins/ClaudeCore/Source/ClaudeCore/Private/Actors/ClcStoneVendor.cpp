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
#include "Animation/AnimMontage.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Actors/ClcOpeningStone.h"
#include "Actors/ClcCuttingStone.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
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
	constexpr float NpcAnimBlendTime = 0.3f;
	const FName NpcAnimSlotName(TEXT("DefaultSlot"));

	/** 从 FText 模板取字符串，替换 {0}=石名 {1}=价格 */
	FString FormatNpcLine(const FText& Template, const FString& StoneName, int32 Price)
	{
		FString S = Template.ToString();
		S.ReplaceInline(TEXT("{0}"), *StoneName);
		S.ReplaceInline(TEXT("{1}"), *FString::FromInt(Price));
		return S;
	}

	/** 仅替换石名（{0}），用于无价格场景（未擦石/纯杂等） */
	FString FormatNpcLineNameOnly(const FText& Template, const FString& StoneName)
	{
		FString S = Template.ToString();
		S.ReplaceInline(TEXT("{0}"), *StoneName);
		return S;
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

	// NPC 台词停留计时——到期自动隐藏对话框
	TickNpcLine(DeltaTime);

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
	DestroyBenchStone();
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
	if (CurrentState != EClcVendorState::StoneOnBench || !BenchStone) return;

	FClcStoneRuntimeData StoneRT;
	const bool bHave = GetBenchStoneData(StoneRT);

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
	if (CurrentState != EClcVendorState::StoneOnBench || !BenchStone) return;

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
	GetBenchStoneData(StoneRT);
	CompleteSellWithPrice(Market->CalculateSalePrice(StoneRT));
}

void AClcStoneVendor::CompleteSellWithPrice(int32 Price)
{
	if (!BenchStone) return;

	// 读最新数据（vendor 不改造石头，数据未变，保持链路一致）
	FClcStoneRuntimeData UpdatedData;
	if (GetBenchStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	// 加金。注意：石头在 PlaceStoneOnVendor 时已从背包移除，这里【绝不】二次 RemoveStone。
	if (CachedBackpack)
	{
		CachedBackpack->AddGold(Price);
	}

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

	// 销毁台上石头
	DestroyBenchStone();

	// 通知蓝图（音效/特效/动画）
	OnStoneSold(ActiveStoneData, Price);

	// NPC 反馈：售出完成
	{
		const int32 SoldTrend = (Price > ActiveStoneData.Internal.PurchasePrice) ? 1
			: (Price < ActiveStoneData.Internal.PurchasePrice) ? -1 : 0;
		const bool bSoldAll = (CachedBackpack && CachedBackpack->GetStones().Num() == 0);

		if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
		{
			if (bSoldAll)
				PendingNpcLine = FormatNpcLine(Cfg->NpcSoldAllLine, ActiveStoneData.DisplayName, Price);
			else if (SoldTrend > 0)
				PendingNpcLine = FormatNpcLine(Cfg->NpcSoldProfitLine, ActiveStoneData.DisplayName, Price);
			else if (SoldTrend < 0)
				PendingNpcLine = FormatNpcLine(Cfg->NpcSoldLossLine, ActiveStoneData.DisplayName, Price);
			else
				PendingNpcLine = FormatNpcLine(Cfg->NpcSoldEvenLine, ActiveStoneData.DisplayName, Price);
		}

		OnNpcSold(SoldTrend, bSoldAll);
	}

	if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
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
		if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
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

		if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
		{
			LT->AddLog(SoldHasMoreTip.ToString(), 2.0f, FLinearColor(0.f, 1.f, 1.f));
		}
	}
}

// ============================================================
// 进入 / 退出展示
// ============================================================

void AClcStoneVendor::EnterSellMode()
{
	if (!CachedPC.IsValid() || !CachedBackpack) return;

	// 背包空 → 拒绝
	if (CachedBackpack->GetStones().Num() == 0)
	{
		if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
		{
			LT->AddLog(EmptyTip.ToString(), 2.0f, FLinearColor::Yellow);
		}
		return;
	}

	CurrentState = EClcVendorState::AwaitingStone;

	// NPC 反馈：进入出售模式
	OnNpcEnterSellMode();
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PendingNpcLine = Cfg->NpcEnterModeLine.ToString();
	}
	if (InteractionIndicator) InteractionIndicator->bHidden = true;

	// 隐藏常驻 UI，避免与出售台 HUD 冲突
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

	OnEnterSellMode();

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

	if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
	{
		LT->AddLog(EnterTip.ToString(), 2.0f, FLinearColor(0.f, 1.f, 1.f));
	}
}

void AClcStoneVendor::SetNpcLine(const FString& Line)
{
	PendingNpcLine = Line;
	NpcLineElapsed = 0.0f;
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
	const bool bExitingWithStone = (CurrentState == EClcVendorState::StoneOnBench);
	if (bExitingWithStone)
	{
		RemoveStoneFromVendor();
	}

	// 恢复 FOV（右键放大可能改过）
	if (VendorCamera) VendorCamera->SetFieldOfView(BaseFOV);

	CurrentState = EClcVendorState::Inactive;

	DestroyVendorHUD();

	// 恢复小白点（玩家还在范围内会自动显示）
	if (InteractionIndicator) InteractionIndicator->bHidden = false;

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

	OnExitSellMode();

	// NPC 反馈：退出出售模式（主动离开；售空退出时已在 OnNpcSold 中处理）
	OnNpcExitSellMode(/*bSoldAll=*/false);
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PendingNpcLine = Cfg->NpcExitModeLine.ToString();
	}

	SetVendorCursor(false);

	if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
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
	BenchStonePhase = ActiveStoneData.Phase;

	// 从背包移除（保持索引一致；石头此刻仅存于 BenchStone + ActiveStoneData）
	CachedBackpack->RemoveStone(StoneIndex);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	const FTransform SpawnTransform = StoneSpawnPoint->GetComponentTransform();
	bool bInitSuccess = false;

	// ---- Phase 分支：解石 → AClcCuttingStone；其他 → AClcOpeningStone ----
	if (BenchStonePhase == EClcStonePhase::Cut)
	{
		AClcCuttingStone* CuttingStone = GetWorld()->SpawnActor<AClcCuttingStone>(
			AClcCuttingStone::StaticClass(),
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnParams);

		BenchStone = CuttingStone;
		if (!CuttingStone)
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] Failed to spawn CuttingStone!"));
			OnNpcStonePlaceFailed();
			if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
			{
				PendingNpcLine = Cfg->NpcStonePlaceFailedLine.ToString();
			}
			return;
		}

		CuttingStone->AttachToComponent(StoneSpawnPoint, FAttachmentTransformRules::KeepWorldTransform);

		const float TargetCoverage = FMath::Max(ActiveStoneData.Internal.BlackRatio,
			ActiveStoneData.Internal.ImpurityRatio + ActiveStoneData.Internal.CrackRatio);
		const float ClampedCoverage = FMath::Clamp(
			TargetCoverage > KINDA_SMALL_NUMBER ? TargetCoverage : FallbackDefectCoverage, 0.01f, 0.95f);
		const int32 DefectCount = FMath::Clamp(2 + FMath::RoundToInt(ClampedCoverage * 4.0f), 2, 5);

		if (!CuttingStone->Initialize(ActiveStoneData, DefectCount, ClampedCoverage,
			VoxelResolution, ShellMaterialPath))
		{
			bInitSuccess = false;
		}
		else
		{
			BenchDisplayMesh = CuttingStone->GetDisplayMesh();
			bInitSuccess = true;
		}

		if (!bInitSuccess)
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] CuttingStone Initialize failed!"));
			CuttingStone->Destroy();
			BenchStone = nullptr;
			BenchDisplayMesh.Reset();
			OnNpcStonePlaceFailed();
			if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
			{
				PendingNpcLine = Cfg->NpcStonePlaceFailedLine.ToString();
			}
			return;
		}
	}
	else
	{
		// Unworked / Windowed / HaggleResolved → 擦石台载体
		AClcOpeningStone* OpStone = GetWorld()->SpawnActor<AClcOpeningStone>(
			AClcOpeningStone::StaticClass(),
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnParams);

		BenchStone = OpStone;
		if (!OpStone)
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] Failed to spawn OpeningStone!"));
			OnNpcStonePlaceFailed();
			if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
			{
				PendingNpcLine = Cfg->NpcStonePlaceFailedLine.ToString();
			}
			return;
		}

		OpStone->AttachToComponent(StoneSpawnPoint, FAttachmentTransformRules::KeepWorldTransform);

		if (!OpStone->Initialize(ActiveStoneData, OpeningMaterialPath))
		{
			bInitSuccess = false;
		}
		else
		{
			BenchDisplayMesh = OpStone->GetStoneMesh();
			bInitSuccess = true;
		}

		if (!bInitSuccess)
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcVendor] OpeningStone Initialize failed!"));
			OpStone->Destroy();
			BenchStone = nullptr;
			BenchDisplayMesh.Reset();
			OnNpcStonePlaceFailed();
			if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
			{
				PendingNpcLine = Cfg->NpcStonePlaceFailedLine.ToString();
			}
			return;
		}
	}

	// 记录初始旋转（出售台自管旋转展示）
	if (BenchDisplayMesh.IsValid())
	{
		BenchInitialRotation = BenchDisplayMesh->GetComponentQuat();
	}

	// 背包开闭状态监听初始化（B 键开关由全局 IA_Backpack 处理，这里只轮询响应）
	bBackpackWasOpen = CachedBackpack->IsBackpackOpen();

	CurrentState = EClcVendorState::StoneOnBench;

	// NPC 反馈：石头上台（判断涨跌+擦石状态）
	{
		int32 PriceTrend = 0;
		int32 OpenStatus = 0; // 0=未开 1=纯杂 2=见玉
		if (ActiveStoneData.OpenedGreenArea > 0.0f)
			OpenStatus = 2;
		else if (ActiveStoneData.OpenedBlackArea > 0.0f)
			OpenStatus = 1;

		int32 SalePrice = 0;
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
			{
				SalePrice = Market->CalculateSalePrice(ActiveStoneData);
				if (SalePrice > ActiveStoneData.Internal.PurchasePrice)
					PriceTrend = 1;
				else if (SalePrice < ActiveStoneData.Internal.PurchasePrice)
					PriceTrend = -1;
			}
		}

		// 选台词
		if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
		{
			if (ActiveStoneData.bHaggleResolved)
				PendingNpcLine = FormatNpcLine(Cfg->NpcStoneLockedLine, ActiveStoneData.DisplayName, ActiveStoneData.HaggleLockedPrice);
			else if (PriceTrend > 0)
				PendingNpcLine = FormatNpcLine(Cfg->NpcStoneProfitLine, ActiveStoneData.DisplayName, SalePrice);
			else if (PriceTrend < 0)
				PendingNpcLine = FormatNpcLine(Cfg->NpcStoneLossLine, ActiveStoneData.DisplayName, SalePrice);
			else if (OpenStatus == 0)
				PendingNpcLine = FormatNpcLineNameOnly(Cfg->NpcStoneUnopenedLine, ActiveStoneData.DisplayName);
			else if (OpenStatus == 1)
				PendingNpcLine = FormatNpcLineNameOnly(Cfg->NpcStoneNoJadeLine, ActiveStoneData.DisplayName);
			else
				PendingNpcLine = FormatNpcLineNameOnly(Cfg->NpcStoneJadeLine, ActiveStoneData.DisplayName);
		}

		OnNpcStonePlaced(PriceTrend, OpenStatus, ActiveStoneData.bHaggleResolved);
	}

	// 立即刷一帧 HUD
	PushVendorHUDData();
	HUDPushTimer = HUDPushInterval;

	if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
	{
		TArray<FStringFormatArg> PlacedArgs;
		PlacedArgs.Add(FStringFormatArg(ActiveStoneData.DisplayName));
		LT->AddLog(FString::Format(*PlacedTip.ToString(), PlacedArgs), 2.0f, FLinearColor::White);
	}

	// 锁价石上台：额外提示玩家直接出手
	if (ActiveStoneData.bHaggleResolved)
	{
		if (UClcLogToastSubsystem* LT2 = ClcGetLogToast(CachedPC))
		{
			LT2->AddLog(TEXT("【锁价石】Enter 直接出手，不可再讨价"), 2.5f, FLinearColor(1.0f, 0.85f, 0.2f));
		}
	}
}

void AClcStoneVendor::RemoveStoneFromVendor()
{
	if (!BenchStone) return;

	// 读最新数据
	FClcStoneRuntimeData UpdatedData;
	if (GetBenchStoneData(UpdatedData))
	{
		ActiveStoneData = UpdatedData;
	}

	// 放回背包
	if (CachedBackpack)
	{
		CachedBackpack->AddStone(ActiveStoneData);
	}

	// 石头已回背包——固化终态，闭合搬运窗口（上台不存档，下台存档）
	SaveAfterStoneReturned();

	DestroyBenchStone();

	ActiveStoneData = FClcStoneRuntimeData();
}

void AClcStoneVendor::DestroyBenchStone()
{
	if (BenchStone)
	{
		BenchStone->Destroy();
		BenchStone = nullptr;
	}
	BenchDisplayMesh.Reset();
	BenchStonePhase = EClcStonePhase::Unworked;
}

bool AClcStoneVendor::GetBenchStoneData(FClcStoneRuntimeData& OutData) const
{
	if (!BenchStone) return false;

	// 按 Phase 分发——两种石头都实现了 GetStoneData
	if (BenchStonePhase == EClcStonePhase::Cut)
	{
		if (AClcCuttingStone* CS = Cast<AClcCuttingStone>(BenchStone))
		{
			return CS->GetStoneData(OutData);
		}
	}
	else
	{
		if (AClcOpeningStone* OS = Cast<AClcOpeningStone>(BenchStone))
		{
			return OS->GetStoneData(OutData);
		}
	}
	return false;
}

void AClcStoneVendor::MarkHaggleResolvedOnActiveStone(int32 LockedPrice)
{
	ActiveStoneData.bHaggleResolved = true;
	ActiveStoneData.HaggleLockedPrice = LockedPrice;

	// 名字加锁价标记（覆盖已有阶段标签，锁价是最高阶段）
	static const FString LockedSuffix = TEXT("【已锁价】");
	static const FString WindowedSuffix = TEXT("【已擦石】");
	static const FString CutSuffix = TEXT("【已解石】");
	if (ActiveStoneData.DisplayName.EndsWith(*WindowedSuffix))
	{
		ActiveStoneData.DisplayName.LeftChopInline(WindowedSuffix.Len());
	}
	else if (ActiveStoneData.DisplayName.EndsWith(*CutSuffix))
	{
		ActiveStoneData.DisplayName.LeftChopInline(CutSuffix.Len());
	}
	if (!ActiveStoneData.DisplayName.EndsWith(*LockedSuffix))
	{
		ActiveStoneData.DisplayName += LockedSuffix;
	}

	// 同步到台上石头
	if (BenchStonePhase == EClcStonePhase::Cut)
	{
		if (AClcCuttingStone* CS = Cast<AClcCuttingStone>(BenchStone))
		{
			CS->MarkHaggleResolved(LockedPrice);
		}
	}
	else
	{
		if (AClcOpeningStone* OS = Cast<AClcOpeningStone>(BenchStone))
		{
			OS->MarkHaggleResolved(LockedPrice);
		}
	}
}

// ============================================================
// 输入处理——WASD 旋转 + R 复位 + 右键放大 + Enter 售出
// ============================================================

void AClcStoneVendor::ProcessStoneOnBenchInput(float DeltaTime)
{
	if (!CachedPC.IsValid() || !BenchDisplayMesh.IsValid()) return;

	// 右键 FOV 放大（纯视觉拉近）
	UpdateAimZoom(DeltaTime);

	// WASD 旋转（相机相对）——出售台自管旋转，不通过石头 Actor 方法
	const float RotAmount = DisplayRotationSpeed * DeltaTime * RotationInputScale;

	float DeltaPitch = 0.0f;
	float DeltaYaw = 0.0f;

	if (CachedPC->IsInputKeyDown(EKeys::W)) DeltaPitch -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::S)) DeltaPitch += RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::A)) DeltaYaw -= RotAmount;
	if (CachedPC->IsInputKeyDown(EKeys::D)) DeltaYaw += RotAmount;

	if (!FMath::IsNearlyZero(DeltaPitch) || !FMath::IsNearlyZero(DeltaYaw))
	{
		bResetRotationPending = false; // 用户手动旋转 → 取消复位

		const FVector CamRight = VendorCamera->GetRightVector();
		const FVector CamUp = VendorCamera->GetUpVector();
		const FQuat CurrentQuat = BenchDisplayMesh->GetComponentQuat();
		const FQuat PitchQuat(CamRight, FMath::DegreesToRadians(DeltaPitch));
		const FQuat YawQuat(CamUp, FMath::DegreesToRadians(DeltaYaw));
		BenchDisplayMesh->SetWorldRotation(YawQuat * PitchQuat * CurrentQuat);
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

	if (bResetRotationPending && BenchDisplayMesh.IsValid())
	{
		const FQuat CurrentQuat = BenchDisplayMesh->GetComponentQuat();
		const FQuat TargetQuat = FMath::QInterpTo(CurrentQuat, BenchInitialRotation, DeltaTime, ResetRotationSpeed);
		BenchDisplayMesh->SetWorldRotation(TargetQuat);
		if (CurrentQuat.Equals(BenchInitialRotation, 0.001f))
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

	// Enter 售出（边沿检测，防连按；放最后，售出后 BenchStone 已销毁）
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

		if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
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

UCameraComponent* AClcStoneVendor::GetAimZoomCamera() const
{
	return VendorCamera;
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

	if (BenchStone)
	{
		FClcStoneRuntimeData StoneRT;
		if (GetBenchStoneData(StoneRT))
		{
			const auto& I = StoneRT.Internal;
			Data.DisplayName   = StoneRT.DisplayName;
			Data.Origin        = I.Origin;
			Data.GradeValue    = (uint8)I.Grade;
			Data.PurchasePrice = I.PurchasePrice;

			// 种水是否暴露：vendor 不改造石头，OpenedGreenArea>0 即已暴露
			Data.bGradeRevealed = StoneRT.OpenedGreenArea > 0.0f;

			// 皮壳名称
			Data.ShellName = UClcShellTextureConfig::GetShellName(I.ShellTypeIndex).ToString();

			// 擦石进度（仅擦石/未加工石头有；解石石头这些字段为 0）
			if (BenchStonePhase != EClcStonePhase::Cut)
			{
				if (AClcOpeningStone* OS = Cast<AClcOpeningStone>(BenchStone))
				{
					float OpenedR, GreenR, BlackR, ImpurityR, CrackR;
					OS->GetOpeningProgress(OpenedR, GreenR, BlackR, ImpurityR, CrackR);
					Data.OpenedRatio = OpenedR;
					Data.SurfaceArea = I.SurfaceArea;
					Data.GreenArea   = GreenR * I.SurfaceArea;
					Data.BlackArea   = BlackR * I.SurfaceArea;
				}
			}
			else
			{
				// 解石石头：HUD 显示体积信息（替代擦石面积）
				Data.OpenedRatio = 0.0f;
				Data.SurfaceArea = I.SurfaceArea;
				Data.GreenArea   = 0.0f;
				Data.BlackArea   = 0.0f;
			}

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

			// 解石体积（解石石头有值；擦石石头为 0）
		Data.ExposedCutVolume = StoneRT.ExposedCutVolume;
		Data.ExposedJadeVolume = StoneRT.ExposedJadeVolume;
		Data.RemainingVolume = StoneRT.RemainingVolume;

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

	// 新台词出现时（与上次推送不同且非空）重置最小显示计时，使每条台词独立享受 ~2s 停留。
	if (!PendingNpcLine.IsEmpty() && PendingNpcLine != LastPushedNpcLine)
	{
		NpcLineElapsed = 0.0f;
	}
	LastPushedNpcLine = PendingNpcLine;
	Data.NpcLine = PendingNpcLine;
	HUDWidget->RefreshData(Data);
	// 不在此清空 PendingNpcLine——由 TickNpcLine 在 NpcLineMinDuration 到期后统一清空，避免 0.3s 周期推送秒清台词。
}

namespace
{
	constexpr float NpcLineMinDuration = 2.0f;
}

void AClcStoneVendor::TickNpcLine(float DeltaTime)
{
	if (NpcLineElapsed < NpcLineMinDuration)
	{
		NpcLineElapsed += DeltaTime;
		// 到期清空待显示台词——下一次 PushVendorHUDData 会把空台词推到 HUD 并保留其他字段
		// （售价/金币等），不再发 default 结构体清空整屏。
		if (NpcLineElapsed >= NpcLineMinDuration)
		{
			PendingNpcLine.Empty();
			LastPushedNpcLine.Empty();
		}
	}
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

			// 进入回收台范围——满足交互条件（背包有石头）才一次性飘字
			if (CachedBackpack && CachedBackpack->GetStones().Num() > 0)
			{
				const double Now = FPlatformTime::Seconds();
				if (Now - LastEnterToastTime > 3.0)
				{
					LastEnterToastTime = Now;
					if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
					{
						LT->AddLog(TEXT("按 F 与收石商谈判"), 2.0f, FLinearColor(0.f, 1.f, 1.f));
					}
				}
			}

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
	// 锁定价写到 ActiveStoneData + 台上石头（出售台自管，不通过石头 Actor 方法）
	MarkHaggleResolvedOnActiveStone(LockedPrice);

	CurrentState = EClcVendorState::StoneOnBench;

	// 立即刷新 HUD 显示锁价
	PushVendorHUDData();

	if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
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
		PlayNpcAnim(Cfg->NpcIdleAnim, true);
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

	GetWorldTimerManager().ClearTimer(NpcReturnIdleTimer);

	UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Anim, NpcAnimSlotName, NpcAnimBlendTime, NpcAnimBlendTime);
	if (!Montage) return;

	NpcMesh->PlayAnimation(Montage, bLoop);

	if (!bLoop)
	{
		const float Len = Anim->GetPlayLength();
		if (Len > 0.0f)
		{
			const float ReturnDelay = FMath::Max(0.01f, Len - NpcAnimBlendTime);
			GetWorldTimerManager().SetTimer(NpcReturnIdleTimer, this, &AClcStoneVendor::ReturnToNpcIdle, ReturnDelay, false);
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

// ============================================================
// 高价值 NPC 反馈事件（C++ 默认实现——从 DA 取对应动画播放；BP 可覆写加音效/特效）
// ============================================================

void AClcStoneVendor::OnNpcStonePlaced_Implementation(int32 PriceTrend, int32 OpenStatus, bool bIsLocked)
{
	UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr;
	if (!Cfg) return;

	// 按优先级选动画：锁价 → 涨跌 → 擦石状态 → 通用
	UAnimSequence* Anim = nullptr;
	if (bIsLocked)
	{
		Anim = Cfg->NpcStonePlacedAnim; // 锁价石走通用
	}
	else if (PriceTrend > 0)
	{
		Anim = Cfg->NpcStoneProfitAnim;
	}
	else if (PriceTrend < 0)
	{
		Anim = Cfg->NpcStoneLossAnim;
	}

	if (!Anim)
	{
		if (OpenStatus == 0)
			Anim = Cfg->NpcStoneUnopenedAnim;
		else if (OpenStatus == 1)
			Anim = Cfg->NpcStoneNoJadeAnim;
		else if (OpenStatus == 2)
			Anim = Cfg->NpcStoneJadeAnim;
	}

	if (!Anim) Anim = Cfg->NpcStonePlacedAnim;
	PlayNpcAnim(Anim, false);
}

void AClcStoneVendor::OnNpcStonePlaceFailed_Implementation()
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcStonePlacedFailedAnim, false);
	}
}

void AClcStoneVendor::OnNpcSold_Implementation(int32 PriceTrend, bool bSoldAll)
{
	UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr;
	if (!Cfg) return;

	PlayNpcAnim(bSoldAll ? Cfg->NpcSoldAllAnim : Cfg->NpcSoldAnim, false);
}

void AClcStoneVendor::OnNpcEnterSellMode_Implementation()
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		PlayNpcAnim(Cfg->NpcEnterModeAnim, false);
	}
}

void AClcStoneVendor::OnNpcExitSellMode_Implementation(bool bSoldAll)
{
	if (UClcHaggleConfig* Cfg = HaggleComponent ? HaggleComponent->GetHaggleConfig() : nullptr)
	{
		// 退出/送客用同一动画，bSoldAll 可未来分叉
		PlayNpcAnim(Cfg->NpcExitModeAnim, false);
	}
}

