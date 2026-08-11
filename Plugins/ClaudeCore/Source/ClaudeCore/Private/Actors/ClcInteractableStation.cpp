// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcInteractableStation.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "UI/ClcBackpackWidget.h"
#include "Components/SphereComponent.h"
#include "Components/SpotLightComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

AClcInteractableStation::AClcInteractableStation()
{
	PrimaryActorTick.bCanEverTick = true;
}

// ---- 缓存引用 ----

void AClcInteractableStation::CachePlayerRefs()
{
	if (APawn* Pawn = PlayerInRange.Get())
	{
		CachedPC = Cast<APlayerController>(Pawn->GetController());
	}

	if (!CachedPC.IsValid()) return;

	if (ULocalPlayer* LP = CachedPC->GetLocalPlayer())
	{
		CachedBackpack = LP->GetSubsystem<UClcBackpackSubsystem>();
	}
}

// ---- 背包选石委托 ----

void AClcInteractableStation::BindToBackpackWidget()
{
	if (!CachedBackpack) return;

	UClcBackpackWidget* Widget = CachedBackpack->GetBackpackWidget();
	if (!Widget) return;

	Widget->OnStoneSelected.RemoveDynamic(this, &AClcInteractableStation::OnBackpackStoneSelected);
	Widget->OnStoneSelected.AddDynamic(this, &AClcInteractableStation::OnBackpackStoneSelected);
}

// ---- 交互选中谓词 ----

bool AClcInteractableStation::QueryCanSelect()
{
	APlayerController* PC = CachedPC.IsValid()
		? CachedPC.Get() : UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->GetLocalPlayer()) return false;

	UClcBackpackSubsystem* Backpack = CachedBackpack
		? CachedBackpack
		: PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>();
	if (!Backpack) return false;

	for (const FClcStoneRuntimeData& Stone : Backpack->GetStones())
	{
		if (IsStoneSelectable(Stone)) return true;
	}
	return false;
}

// ---- 自适应补光（三处逐字相同的实现，下沉到基类） ----

void AClcInteractableStation::TickFillLight(float DeltaTime)
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

// ---- 右键 FOV 放大 ----

void AClcInteractableStation::UpdateAimZoom(float DeltaTime)
{
	UCameraComponent* Camera = GetAimZoomCamera();
	if (!Camera) return;

	const bool bAimDown = CachedPC.IsValid() && CachedPC->IsInputKeyDown(EKeys::RightMouseButton);
	const float TargetFOV = bAimDown ? (BaseFOV / AimZoomFactor) : BaseFOV;
	const float CurrentFOV = Camera->FieldOfView;
	const float NewFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, AimZoomSpeed);
	Camera->SetFieldOfView(NewFOV);
}

// ---- 虚钩子默认实现 ----

void AClcInteractableStation::UpdateFillLightTarget()
{
	// 默认空——子类按自己的状态枚举 override
}

bool AClcInteractableStation::IsStoneSelectable(const FClcStoneRuntimeData& Stone) const
{
	return true;
}

UCameraComponent* AClcInteractableStation::GetAimZoomCamera() const
{
	return nullptr;
}

UClcBackpackWidget* AClcInteractableStation::GetBackpackWidget() const
{
	return CachedBackpack ? CachedBackpack->GetBackpackWidget() : nullptr;
}

void AClcInteractableStation::OnBackpackStoneSelected(int32 StoneIndex)
{
	// 默认空——子类 override 实现上台/换石
}
