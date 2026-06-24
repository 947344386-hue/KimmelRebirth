// Copyright ClaudeCore. All Rights Reserved.
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcInteractionWidget.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
UClcInteractionIndicator::UClcInteractionIndicator() { PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=0.1f; }
void UClcInteractionIndicator::BeginPlay() {
	Super::BeginPlay();
	WidgetClass = LoadClass<UClcInteractionWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_InteractionIndicator.WBP_InteractionIndicator_C"));
	if (!WidgetClass) { UE_LOG(LogTemp, Error, TEXT("[ClcIndicator] Failed to load!")); return; }
	WidgetComp = NewObject<UWidgetComponent>(GetOwner(), UWidgetComponent::StaticClass());
	if (WidgetComp) {
		WidgetComp->SetWidgetClass(WidgetClass); WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
		WidgetComp->SetDrawSize(FVector2D(48,48)); WidgetComp->SetRelativeLocation(WidgetOffset);
		WidgetComp->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		WidgetComp->RegisterComponent();
		InteractionWidget = Cast<UClcInteractionWidget>(WidgetComp->GetUserWidgetObject());
	}
	UpdateInteractionState();
}
void UClcInteractionIndicator::TickComponent(float DT, ELevelTick, FActorComponentTickFunction*) { Super::TickComponent(DT, ELevelTick(), nullptr); UpdateInteractionState(); }
void UClcInteractionIndicator::UpdateInteractionState() {
	if (!InteractionWidget||!WidgetComp) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); if (!PC) return;
	APawn* Pawn = PC->GetPawn(); if (!Pawn) return;
	if (FVector::Dist(Pawn->GetActorLocation(), GetOwner()->GetActorLocation()) > InteractionRadius) {
		if (CurrentState!=0) { InteractionWidget->SetStateHidden(); WidgetComp->SetVisibility(false); CurrentState=0; }
		return;
	}
	WidgetComp->SetVisibility(true);
	bool bAim=false;
	if (APlayerCameraManager* CM=PC->PlayerCameraManager) {
		FVector CL=CM->GetCameraLocation(), CD=CM->GetCameraRotation().Vector();
		FCollisionQueryParams P; P.AddIgnoredActor(Pawn); P.AddIgnoredActor(GetOwner()->GetAttachParentActor());
		TArray<FHitResult> H;
		if (GetWorld()->LineTraceMultiByChannel(H, CL, CL+CD*10000, ECC_Visibility, P))
			for (auto& HH : H) if (HH.GetActor()==GetOwner()) { bAim=true; break; }
	}
	if (bAim) { if (CurrentState!=2) { InteractionWidget->SetStateSelected(); CurrentState=2; } }
	else { if (CurrentState!=1) { InteractionWidget->SetStateInRange(); CurrentState=1; } }
}