// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/Teleport/ClcTeleportPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/DrawSphereComponent.h"
#include "Components/SceneComponent.h"

AClcTeleportPoint::AClcTeleportPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	SelectionSphere = CreateEditorOnlyDefaultSubobject<UDrawSphereComponent>(TEXT("SelectionSphere"));
	if (SelectionSphere)
	{
		SelectionSphere->SetupAttachment(SceneRoot);
		SelectionSphere->SetSphereRadius(75.0f);
		SelectionSphere->ShapeColor = FColor(40, 200, 255);
		SelectionSphere->bDrawOnlyIfSelected = false;
		SelectionSphere->SetLineThickness(2.5f);
		SelectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionSphere->SetGenerateOverlapEvents(false);
		SelectionSphere->SetCanEverAffectNavigation(false);
		SelectionSphere->SetHiddenInGame(true);
		SelectionSphere->SetIsVisualizationComponent(true);
	}

	ArrivalArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrivalArrow"));
	if (ArrivalArrow)
	{
		ArrivalArrow->SetupAttachment(SceneRoot);
		ArrivalArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrivalArrow->SetGenerateOverlapEvents(false);
		ArrivalArrow->SetCanEverAffectNavigation(false);
		ArrivalArrow->SetHiddenInGame(true);
		ArrivalArrow->SetIsVisualizationComponent(true);
		ArrivalArrow->ArrowColor = FColor(40, 200, 255);
		ArrivalArrow->ArrowSize = 1.5f;
		ArrivalArrow->bTreatAsASprite = true;
		ArrivalArrow->bIsScreenSizeScaled = true;
	}
#endif
}

FText AClcTeleportPoint::GetDisplayName() const
{
	return DisplayName.IsEmpty() ? NSLOCTEXT("ClcTeleport", "UnnamedPoint", "未命名传送点") : DisplayName;
}
