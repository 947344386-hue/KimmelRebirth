#include "Components/JBJadeInteractionComponent.h"
#include "Actors/JBJadeStone.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"

UJBJadeInteractionComponent::UJBJadeInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UJBJadeInteractionComponent::SetEnabled(bool bEnable)
{
	bInteractionEnabled = bEnable;
	if (!bEnable) { bIsPainting = false; bIsZooming = false; }
}

void UJBJadeInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bInteractionEnabled) return;
	if (bIsPainting && TargetStone) PaintAtCursor();
	UpdateCameraZoom(DeltaTime);
}

void UJBJadeInteractionComponent::StartPaint() { if (!bInteractionEnabled) return; bIsPainting = true; LastPaintUV = FVector2D(-1.0f, -1.0f); }
void UJBJadeInteractionComponent::StopPaint()  { bIsPainting = false; }
void UJBJadeInteractionComponent::StartZoom()  { if (bInteractionEnabled) bIsZooming = true; }
void UJBJadeInteractionComponent::StopZoom()   { bIsZooming = false; }

void UJBJadeInteractionComponent::PaintAtCursor()
{
	FVector2D HitUV;
	if (!TraceCrust(HitUV)) return;
	if (LastPaintUV.X >= 0.0f && FVector2D::Distance(HitUV, LastPaintUV) < PaintStepThreshold) return;
	LastPaintUV = HitUV;
	TargetStone->PaintMaskAtUV(HitUV, BrushRadius);
}

bool UJBJadeInteractionComponent::TraceCrust(FVector2D& OutUV) const
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !TargetStone) return false;
	UStaticMeshComponent* Crust = TargetStone->GetCrustMesh();
	if (!Crust || !Crust->GetStaticMesh()) return false;
	float MouseX, MouseY;
	if (!PC->GetMousePosition(MouseX, MouseY)) return false;
	FVector WorldOrigin, WorldDirection;
	if (!UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(MouseX, MouseY), WorldOrigin, WorldDirection)) return false;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = true;
	QueryParams.AddIgnoredActor(PC->GetPawn());
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, WorldOrigin + WorldDirection * 10000.0f, ECC_Visibility, QueryParams)) return false;
	if (Hit.GetComponent() != Crust) return false;
	if (UGameplayStatics::FindCollisionUV(Hit, 0, OutUV)) return true;
	const FVector LocalPos = Crust->GetComponentTransform().InverseTransformPosition(Hit.Location);
	const FVector Dir = LocalPos.GetSafeNormal();
	OutUV.X = 0.5f + FMath::Atan2(Dir.Y, Dir.X) / (2.0f * PI);
	OutUV.Y = 0.5f + FMath::Asin(Dir.Z) / PI;
	return true;
}

void UJBJadeInteractionComponent::UpdateCameraZoom(float DeltaTime)
{
	if (!TargetCamera) return;
	const float TargetFOV = bIsZooming ? ZoomFOV : NormalFOV;
	const float CurrentFOV = TargetCamera->FieldOfView;
	if (FMath::IsNearlyEqual(CurrentFOV, TargetFOV, 0.1f)) { TargetCamera->SetFieldOfView(TargetFOV); return; }
	TargetCamera->SetFieldOfView(FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, ZoomInterpSpeed));
}