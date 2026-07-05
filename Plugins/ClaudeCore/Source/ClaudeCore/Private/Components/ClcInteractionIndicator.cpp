// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcInteractionIndicator.h"
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
		UE_LOG(LogTemp, Error, TEXT("[ClcIndicator] Failed to load WBP_InteractionIndicator!"));
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
	UpdateInteractionState();
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
		// 瞄准模式：摄像机射线命中 Owner 才算选中
		APlayerCameraManager* CamMgr = PC->PlayerCameraManager;
		if (CamMgr)
		{
			const FVector CamLoc = CamMgr->GetCameraLocation();
			const FVector CamDir = CamMgr->GetCameraRotation().Vector();

			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Pawn);
			Params.AddIgnoredActor(GetOwner()->GetAttachParentActor());

			TArray<FHitResult> Hits;
			if (GetWorld()->LineTraceMultiByChannel(Hits, CamLoc, CamLoc + CamDir * 10000.0f,
				ECC_Visibility, Params))
			{
				for (const FHitResult& H : Hits)
				{
					if (H.GetActor() == GetOwner())
					{
						bSelected = true;
						break;
					}
				}
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
