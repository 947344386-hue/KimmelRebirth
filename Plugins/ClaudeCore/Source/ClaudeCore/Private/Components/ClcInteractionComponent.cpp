// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcInteractionComponent.h"
#include "UI/ClcUILayers.h"
#include "ClcLog.h"
#include "Interfaces/ClcInteractable.h"
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcInteractionWidget.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"

DECLARE_CYCLE_STAT(TEXT("Interaction Update"), STAT_Interaction_Update, STATGROUP_Game);

UClcInteractionComponent::UClcInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UClcInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

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
				ReticleWidget->AddToViewport(FClcUIZOrder::Reticle);
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
	SCOPE_CYCLE_COUNTER(STAT_Interaction_Update);
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

	// 准星延迟创建
	if (!ReticleWidget && ReticleWidgetClass)
	{
		ReticleWidget = CreateWidget<UClcInteractionWidget>(PC, ReticleWidgetClass);
		if (ReticleWidget)
		{
			ReticleWidget->AddToViewport(FClcUIZOrder::Reticle);
			ReticleWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
			ReticleWidget->SetStateHidden();
		}
	}

	// ---- 1. 唯一一条中心球扫：CurrentLookedAtActor ----
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
	AActor* AimSelected = nullptr;
	AActor* ProximitySelected = nullptr;

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
			bThisSelected = Ind->OnQueryCanSelect.IsBound() ? Ind->OnQueryCanSelect.Execute() : true;
			if (bThisSelected) ProximitySelected = A;
		}
		else
		{
			bThisSelected = (A == LookedAt);
			if (bThisSelected) AimSelected = A;
		}

		if (Ind) Ind->ApplyControllerState(bThisSelected ? 2 : 1);
	}

	AActor* Selected = AimSelected ? AimSelected : ProximitySelected;
	CurrentSelectedActor = Selected;

	// ---- 3. 按键提示：所有 IClcInteractable 统一由本组件管理 ----
	// 工作台/解石台/修理站/升级站/回收商/石头 只需实现 IClcInteractable::GetInteractionPrompt，
	// F 键路由由这里统一处理；各站点不再各自维护 PromptHandle 和 Tick 按键轮询。
	{
		const bool bShouldShow = !bInExclusiveContext && Selected != nullptr;
		FText PromptText = FText::GetEmpty();
		if (bShouldShow)
		{
			if (IClcInteractable* I = Cast<IClcInteractable>(Selected))
			{
				PromptText = I->GetInteractionPrompt();
			}
		}

		if (bShouldShow && SelectedPromptHandle == 0)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
				{
					SelectedPromptHandle = KP->RegisterKeyPrompt(
						EKeys::F, PromptText, Selected->GetClass()->GetFName(), 100);
				}
			}
		}
		else if (bShouldShow && Selected != LastSelectedActor.Get())
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
				{
					KP->UpdateKeyPromptLabel(SelectedPromptHandle, PromptText);
				}
			}
		}
		else if (!bShouldShow && SelectedPromptHandle != 0)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
				{
					KP->UnregisterKeyPrompt(SelectedPromptHandle);
				}
			}
			SelectedPromptHandle = 0;
		}
		LastSelectedActor = Selected;
	}

	// ---- 4. F 键路由：统一处理交互输入 ----
	if (bAnyInRange)
	{
		HandleInteractInput();
	}

	// ---- 5. 准星态 + Prompt 文案 ----
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

	if (State == 2 || bStateChanged)
	{
		ReticleWidget->SetPromptText(Prompt);
	}
}

// ============================================================
// 统一 F 键路由
// ============================================================

void UClcInteractionComponent::SetExclusiveContext(bool bExclusive)
{
	bInExclusiveContext = bExclusive;
	// 进入独占上下文 → 立即注销提示；退出时下一 tick 重新注册
	if (bExclusive && SelectedPromptHandle != 0)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
				{
					KP->UnregisterKeyPrompt(SelectedPromptHandle);
				}
			}
		}
		SelectedPromptHandle = 0;
	}
	bInteractKeyPrev = false;
}

void UClcInteractionComponent::HandleInteractInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	const bool bKeyDown = PC->IsInputKeyDown(EKeys::F);
	const bool bJustPressed = bKeyDown && !bInteractKeyPrev;
	bInteractKeyPrev = bKeyDown;

	if (!bJustPressed) return;

	AActor* Selected = CurrentSelectedActor.Get();
	if (!Selected) return;

	if (IClcInteractable* Interactable = Cast<IClcInteractable>(Selected))
	{
		APawn* Pawn = PC->GetPawn();
		const bool bSuccess = Interactable->OnInteract(Pawn);

		// 交互失败 → 飘 Toast（各站点 OnInteract 内也会飘详细原因，这里做兜底）
		if (!bSuccess)
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UClcLogToastSubsystem* Toast = LP->GetSubsystem<UClcLogToastSubsystem>())
				{
					Toast->AddLog(TEXT("无法交互"), 1.5f, FLinearColor::Yellow);
				}
			}
		}
	}
}
