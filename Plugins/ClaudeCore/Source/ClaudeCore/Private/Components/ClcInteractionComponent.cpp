// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcInteractionComponent.h"
#include "ClcLog.h"
#include "Interfaces/ClcInteractable.h"
#include "Actors/ClcStone.h"
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcInteractionWidget.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"

UClcInteractionComponent::UClcInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UClcInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 准星 Widget 类兜底：未在 BP Details 指定时按约定路径加载 WBP_Reticle。
	// 资产未创建前 LoadClass 返回 null——准星不显示，但中心 trace / Indicator 驱动 / 商人收敛照常生效。
	if (!ReticleWidgetClass)
	{
		ReticleWidgetClass = LoadClass<UClcInteractionWidget>(
			nullptr, TEXT("/Game/JadeBetting/UI/WBP_Reticle.WBP_Reticle_C"));
	}

	if (ReticleWidgetClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			ReticleWidget = CreateWidget<UClcInteractionWidget>(PC, ReticleWidgetClass);
			if (ReticleWidget)
			{
				ReticleWidget->AddToViewport(10);
				// 锚定屏幕中心——与相机正前方射线发射点对齐。
				ReticleWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
				ReticleWidget->SetStateHidden();
			}
		}
	}
	else
	{
		UE_LOG(LogClaudeCore, Log,
			TEXT("[ClcInteraction] WBP_Reticle 未配置/未创建——准星不显示，其余收敛逻辑正常。"));
	}
}

void UClcInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateInteraction();
}

void UClcInteractionComponent::UpdateInteraction()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		SetReticleState(0, FText::GetEmpty());
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		SetReticleState(0, FText::GetEmpty());
		return;
	}

	// 准星延迟创建：BeginPlay 时 PC 可能尚未就绪，此处兜底，避免准星永远不出现。
	if (!ReticleWidget && ReticleWidgetClass)
	{
		ReticleWidget = CreateWidget<UClcInteractionWidget>(PC, ReticleWidgetClass);
		if (ReticleWidget)
		{
			ReticleWidget->AddToViewport(10);
			ReticleWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
			ReticleWidget->SetStateHidden();
		}
	}

	// ---- 1. 唯一一条中心球扫：CurrentLookedAtActor ----
	// SweepSingle 返回最近命中——与旧 Indicator/商人自检一致的遮挡语义（不穿透墙）。
	AActor* LookedAt = nullptr;
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector TraceEnd = CamLoc + CamRot.Vector() * LookDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	const float SweepR = FMath::Max(0.0f, ReticleSweepRadius);
	const bool bHit = (SweepR > SMALL_NUMBER)
		? GetWorld()->SweepSingleByChannel(Hit, CamLoc, TraceEnd, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(SweepR), Params)
		: GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params);

	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UClcInteractable>())
		{
			LookedAt = HitActor;
		}
	}
	CurrentLookedAtActor = LookedAt;

	// ---- 2. 收集附近交互物，按各自 InteractionRadius 判 in-range，驱动各 Indicator 态 ----
	// 交互物缓存每秒重建一次（GetAllActorsWithInterface + FindComponentByClass 较重），其余 tick 复用。
	if (FPlatformTime::Seconds() - InteractableCacheRebuildTime >= 1.0)
	{
		InteractableCacheRebuildTime = FPlatformTime::Seconds();
		CachedInteractables.Reset();
		CachedIndicators.Reset();
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsWithInterface(GetWorld(), UClcInteractable::StaticClass(), Found);
		for (AActor* A : Found)
		{
			CachedInteractables.Add(A);
			CachedIndicators.Add(A->FindComponentByClass<UClcInteractionIndicator>());
		}
	}

	const FVector PawnLoc = Pawn->GetActorLocation();

	bool bAnyInRange = false;
	AActor* AimSelected = nullptr;       // aim 模式命中且 in-range（准星优先）
	AActor* ProximitySelected = nullptr; // proximity 委托 true（准星次选）

	for (int32 Ci = 0; Ci < CachedInteractables.Num(); ++Ci)
	{
		AActor* A = CachedInteractables[Ci].Get();
		if (!A) continue;
		UClcInteractionIndicator* Ind = CachedIndicators[Ci].Get();
		const float Dist = FVector::Dist(PawnLoc, A->GetActorLocation());
		const float Radius = Ind ? Ind->InteractionRadius : 0.0f;

		if (Dist > Radius)
		{
			if (Ind) Ind->ApplyControllerState(0);
			continue;
		}

		bAnyInRange = true;

		bool bThisSelected = false;
		if (Ind && Ind->bSelectByProximity)
		{
			// proximity 模式（工作台/回收商）：不要求瞄准，由 Owner 委托决定选中（背包有石头→选中）。
			bThisSelected = Ind->OnQueryCanSelect.IsBound() ? Ind->OnQueryCanSelect.Execute() : true;
			if (bThisSelected) ProximitySelected = A;
		}
		else
		{
			// aim 模式（石头）：中心射线命中自己才算选中。
			bThisSelected = (A == LookedAt);
			if (bThisSelected) AimSelected = A;
		}

		if (Ind) Ind->ApplyControllerState(bThisSelected ? 2 : 1);
	}

	// 准星选中：aim 命中优先，否则取 proximity 委托选中。
	AActor* Selected = AimSelected ? AimSelected : ProximitySelected;
	CurrentSelectedActor = Selected;

	// ---- 按键提示：选中可购买原石（AClcStone）时显示 E，取消选中时隐藏 ----
	// 工作台/回收商用 F（各自 overlap 注册 E 之外的键），这里只管 E。
	{
		const bool bShouldShowEPrompt = (Selected && Selected->IsA(AClcStone::StaticClass()));
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
			{
				if (bShouldShowEPrompt && InteractPromptHandle == 0)
				{
					InteractPromptHandle = KP->RegisterKeyPrompt(
						EKeys::E,
						NSLOCTEXT("ClcInteraction", "PurchasePromptLabel", "购买原石"),
						FName("Purchase"), 150);
				}
				else if (!bShouldShowEPrompt && InteractPromptHandle != 0)
				{
					KP->UnregisterKeyPrompt(InteractPromptHandle);
					InteractPromptHandle = 0;
				}
			}
		}
	}

	// ---- 3. 准星态 + Prompt 文案 ----
	if (!bAnyInRange)
	{
		SetReticleState(0, FText::GetEmpty());
	}
	else if (Selected)
	{
		FText Prompt = FText::GetEmpty();
		if (IClcInteractable* I = Cast<IClcInteractable>(Selected))
		{
			Prompt = I->GetInteractionPrompt();
		}
		SetReticleState(2, Prompt);
	}
	else
	{
		SetReticleState(1, FText::GetEmpty());
	}
}

void UClcInteractionComponent::SetReticleState(int32 State, const FText& Prompt)
{
	if (!ReticleWidget) return;

	const bool bStateChanged = (CurrentReticleState != State);
	CurrentReticleState = State;

	if (bStateChanged)
	{
		switch (State)
		{
		case 0: ReticleWidget->SetStateHidden(); break;
		case 1: ReticleWidget->SetStateInRange(); break;
		case 2: ReticleWidget->SetStateSelected(); break;
		default: break;
		}
	}

	// 选中态每帧刷新 Prompt（价格/石头可能变）；其余态仅在状态变化时推一次（清屏或无文案）。
	if (State == 2 || bStateChanged)
	{
		ReticleWidget->SetPromptText(Prompt);
	}
}
