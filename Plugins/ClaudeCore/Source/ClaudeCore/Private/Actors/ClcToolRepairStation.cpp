// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcToolRepairStation.h"
#include "ClcLog.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Components/ClcInteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

AClcToolRepairStation::AClcToolRepairStation()
{
	// 交互提示和 F 键路由已下沉到 UClcInteractionComponent，本站无 Tick 工作
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.TickInterval = 0.0f;

	// 无缩放根
	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	RootComponent = StationRoot;

	// 视觉 Mesh（BP 中设置）—— QueryOnly 碰撞让交互组件中心球扫能命中
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(StationRoot);
	StationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	StationMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StationMesh->SetGenerateOverlapEvents(false);

	// 范围触发器
	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(StationRoot);
	TriggerSphere->InitSphereRadius(InteractionRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 交互指示器（小白点）—— aim 模式：只有摄像机球扫命中才算选中
	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
	InteractionIndicator->bSelectByProximity = false;
	InteractionIndicator->InteractionRadius = InteractionRadius;
}

void AClcToolRepairStation::BeginPlay()
{
	Super::BeginPlay();

	// 同步触发器半径
	TriggerSphere->SetSphereRadius(InteractionRadius);
	InteractionIndicator->InteractionRadius = InteractionRadius;

	// 绑定重叠
	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcToolRepairStation::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcToolRepairStation::OnTriggerEndOverlap);

	// aim 模式下选中完全由交互组件中心球扫决定，不需要 OnQueryCanSelect 委托
}


void AClcToolRepairStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// F 键提示已由 UClcInteractionComponent 统一管理
	Super::EndPlay(EndPlayReason);
}

// ---- IClcInteractable ----

FText AClcToolRepairStation::GetInteractionPrompt() const
{
	return BuildInteractionPrompt();
}

bool AClcToolRepairStation::OnInteract(AActor* Interactor)
{
	// 预留：若未来交互组件统一路由，由此转发
	if (APlayerController* PC = Interactor ? Interactor->GetWorld()->GetFirstPlayerController() : nullptr)
	{
		ExecuteRepair(PC);
		return true;
	}
	return false;
}

// ---- 修复逻辑 ----

bool AClcToolRepairStation::HasToolsNeedingRepair() const
{
	if (RepairableTools == 0) return false;

	UClcToolDurabilitySubsystem* DuraSys = UClcToolDurabilitySubsystem::Get(GetWorld());
	if (!DuraSys) return false;

	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Opener))
	{
		if (DuraSys->NeedsRepair(EClcRepairableTool::Opener)) return true;
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Flashlight))
	{
		if (DuraSys->NeedsRepair(EClcRepairableTool::Flashlight)) return true;
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Combined))
	{
		if (DuraSys->NeedsRepair(EClcRepairableTool::Combined)) return true;
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Blade))
	{
		if (DuraSys->NeedsRepair(EClcRepairableTool::Blade)) return true;
	}
	return false;
}

void AClcToolRepairStation::ExecuteRepair(APlayerController* PC)
{
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	UClcBackpackSubsystem* Backpack = LP->GetSubsystem<UClcBackpackSubsystem>();
	UClcToolDurabilitySubsystem* DuraSys = LP->GetSubsystem<UClcToolDurabilitySubsystem>();

	if (!Backpack)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcToolRepairStation] No BackpackSubsystem found!"));
		return;
	}

	// ── 反馈类检查（不拦执行，只反馈）──
	// 耐久满 → 飘字告知，不扣钱
	if (!HasToolsNeedingRepair())
	{
		OnRepairFailed_NoToolsNeedRepair();
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(PC))
		{
			Toast->AddLog(TEXT("工具耐久已满，无需修复"), 2.0f);
		}
		return;
	}

	// 金币不足 → 飘字告知，不扣钱
	const int32 CurrentGold = Backpack->GetGold();
	if (CurrentGold < RepairCost)
	{
		OnRepairFailed_NotEnoughGold(RepairCost, CurrentGold);
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(PC))
		{
			Toast->AddLog(FString::Printf(TEXT("金币不足！需要 %d，当前 %d"), RepairCost, CurrentGold), 2.5f);
		}
		return;
	}

	// 扣金币
	if (!Backpack->SpendGold(RepairCost))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcToolRepairStation] SpendGold failed despite gold check passing!"));
		return;
	}

	// 恢复耐久
	if (DuraSys)
	{
		DuraSys->RestoreDurabilityMask(RepairableTools);
	}

	// Toast 提示
	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(PC))
	{
		const FString Names = BuildToolNamesString();
		Toast->AddLog(FString::Printf(TEXT("已修复 %s，花费 %d 金币"), *Names, RepairCost), 2.5f);
	}

	// 蓝图事件：音效/特效
	OnRepairSuccess(RepairCost);

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcToolRepairStation] Repair success: cost=%d, tools=%s"), RepairCost, *LexToString(RepairableTools));
}

// ---- 蓝图事件默认实现 ----

void AClcToolRepairStation::OnRepairSuccess_Implementation(int32 CostPaid) {}
void AClcToolRepairStation::OnRepairFailed_NotEnoughGold_Implementation(int32 RequiredGold, int32 CurrentGold) {}
void AClcToolRepairStation::OnRepairFailed_NoToolsNeedRepair_Implementation() {}

// ---- 辅助函数 ----

FString AClcToolRepairStation::BuildToolNamesString() const
{
	TArray<FString> Names;
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Opener))
	{
		Names.Add(TEXT("擦石器"));
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Flashlight))
	{
		Names.Add(TEXT("手电筒"));
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Combined))
	{
		Names.Add(TEXT("手电擦石器"));
	}
	if (RepairableTools & static_cast<int32>(EClcRepairableTool::Blade))
	{
		Names.Add(TEXT("解石刀"));
	}
	if (Names.IsEmpty())
	{
		return TEXT("（无）");
	}
	return FString::Join(Names, TEXT("、"));
}

FText AClcToolRepairStation::BuildInteractionPrompt() const
{
	// 如果用户手动配了提示文本，优先用
	if (!InteractionPrompt.IsEmpty())
	{
		return InteractionPrompt;
	}

	// 自动生成：按 F 修复擦石器、手电筒
	const FString KeyStr = EnterKey.ToString();
	const FString Names = BuildToolNamesString();
	return FText::FromString(FString::Printf(TEXT("按 %s 修复%s"), *KeyStr, *Names));
}

// IsLookedAtByPlayer / CachedInteractionComp 已删除——F 键路由由 UClcInteractionComponent 统一接管

// ---- 重叠 ----

void AClcToolRepairStation::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!Other->IsA<APawn>()) return;
	APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	bPlayerInRange = true;
	CachedPC = Cast<APlayerController>(Pawn->GetController());

	// 进入修理站范围——满足交互条件（有工具需要修复）才一次性飘字提示按 F 修复工具
	if (CachedPC.IsValid() && HasToolsNeedingRepair())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - LastEnterToastTime > 3.0)
		{
			LastEnterToastTime = Now;
			if (UClcLogToastSubsystem* LT = ClcGetLogToast(CachedPC))
			{
				LT->AddLog(FString::Printf(TEXT("按 F 修复工具：%s"), *BuildToolNamesString()), 2.0f, FLinearColor(0.f, 1.f, 1.f));
			}
		}
	}
}

void AClcToolRepairStation::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!Other->IsA<APawn>()) return;
	APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	bPlayerInRange = false;

	// F 键提示已由 UClcInteractionComponent 统一管理
	CachedPC.Reset();
}

// aim 模式下选中完全由交互组件中心球扫决定，QueryCanSelect 不被调用，无需实现