// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcInteractionIndicator.h"
#include "ClcLog.h"
#include "UI/ClcInteractionWidget.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

UClcInteractionIndicator::UClcInteractionIndicator()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UClcInteractionIndicator::BeginPlay()
{
	Super::BeginPlay();

	if (!WidgetClass) { WidgetClass = LoadClass<UClcInteractionWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_InteractionIndicator.WBP_InteractionIndicator_C")); }

	if (!WidgetClass)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcIndicator] Failed to load WBP_InteractionIndicator!"));
		return;
	}

	WidgetComp = NewObject<UWidgetComponent>(GetOwner(), UWidgetComponent::StaticClass());
	if (WidgetComp)
	{
		WidgetComp->SetWidgetClass(WidgetClass);
		WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
		WidgetComp->SetDrawSize(FVector2D(48.0f, 48.0f));
		WidgetComp->SetRelativeLocation(WidgetOffset);
		WidgetComp->AttachToComponent(GetOwner()->GetRootComponent(),
			FAttachmentTransformRules::KeepRelativeTransform);
		WidgetComp->RegisterComponent();

		InteractionWidget = Cast<UClcInteractionWidget>(WidgetComp->GetUserWidgetObject());
	}

	UpdateInteractionState();
}

void UClcInteractionIndicator::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 收敛后视觉态由角色侧 UClcInteractionComponent 统一驱动（ApplyControllerState）。
	// 最近 0.5s 内被驱动过则不自检，避免与中心组件双重 trace；
	// 角色未挂中心组件或组件停止 Tick 时 0.5s 超时，回退到下方旧自检逻辑，保证不黑屏。
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastDrivenTime > 0.0 && (Now - LastDrivenTime) < 0.5)
	{
		return;
	}
	UpdateInteractionState();
}

void UClcInteractionIndicator::ApplyControllerState(int32 NewState)
{
	if (!InteractionWidget || !WidgetComp) return;

	// 记录驱动时刻——Tick 据此判断是否回退自检。
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	LastDrivenTime = Now;

	// 强制隐藏——Owner 进特殊模式时设 bHidden=true
	if (bHidden)
	{
		if (CurrentState != 0)
		{
			InteractionWidget->SetStateHidden();
			WidgetComp->SetVisibility(false);
			CurrentState = 0;
		}
		return;
	}

	if (CurrentState == NewState) return;
	CurrentState = NewState;

	switch (NewState)
	{
	case 0:
		InteractionWidget->SetStateHidden();
		WidgetComp->SetVisibility(false);
		break;
	case 1:
		WidgetComp->SetVisibility(true);
		InteractionWidget->SetStateInRange();
		break;
	case 2:
		WidgetComp->SetVisibility(true);
		InteractionWidget->SetStateSelected();
		break;
	default:
		break;
	}
}

void UClcInteractionIndicator::UpdateInteractionState()
{
	if (!InteractionWidget || !WidgetComp) return;

	// 强制隐藏——Owner 进特殊模式时设 bHidden=true
	if (bHidden)
	{
		if (CurrentState != 0)
		{
			InteractionWidget->SetStateHidden();
			WidgetComp->SetVisibility(false);
			CurrentState = 0;
		}
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	const float Dist = FVector::Dist(Pawn->GetActorLocation(), GetOwner()->GetActorLocation());

	if (Dist > InteractionRadius)
	{
		if (CurrentState != 0)
		{
			InteractionWidget->SetStateHidden();
			WidgetComp->SetVisibility(false);
			CurrentState = 0;
		}
		return;
	}

	WidgetComp->SetVisibility(true);

	bool bSelected = false;

	if (bSelectByProximity)
	{
		// 范围选中模式：不要求瞄准，由 Owner 委托决定是否可选（未绑定=纯距离选中）
		bSelected = OnQueryCanSelect.IsBound() ? OnQueryCanSelect.Execute() : true;
	}
	else
	{
		// 瞄准模式：摄像机方向命中 Owner 才算选中。
		// 用球扫（SweepSingleByChannel）替代细射线——放宽命中，越肩偏高视角下不必把摄像机压很低，
		// 且球比线粗，不易被石头前缘/摊位边/地面遮挡。AimSweepRadius=0 时退回细射线（兼容）。
		APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
		if (CamMgr)
		{
			const FVector CamLoc = CamMgr->GetCameraLocation();
			const FVector CamDir = CamMgr->GetCameraRotation().Vector();
			const FVector TraceEnd = CamLoc + CamDir * InteractionRadius * 1.5f;

			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Pawn);
			Params.AddIgnoredActor(GetOwner()->GetAttachParentActor());

			FHitResult Hit;
			const float SweepR = FMath::Max(0.0f, AimSweepRadius);
			const bool bHit = (SweepR > SMALL_NUMBER)
				? GetWorld()->SweepSingleByChannel(Hit, CamLoc, TraceEnd, FQuat::Identity,
					ECC_Visibility, FCollisionShape::MakeSphere(SweepR), Params)
				: GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params);

			if (bHit && Hit.GetActor() == GetOwner())
			{
				bSelected = true;
			}
		}
	}

	if (bSelected)
	{
		if (CurrentState != 2)
		{
			InteractionWidget->SetStateSelected();
			CurrentState = 2;
		}
	}
	else
	{
		if (CurrentState != 1)
		{
			InteractionWidget->SetStateInRange();
			CurrentState = 1;
		}
	}
}
