// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcStoneInfoWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UClcStoneInfoWidget::SetAnchor(AActor* InAnchor, const FVector& InWorldOffset)
{
	AnchorActor = InAnchor;
	AnchorWorldOffset = InWorldOffset;
}

void UClcStoneInfoWidget::UpdateScreenPosition()
{
	if (!AnchorActor.IsValid())
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	const FVector WorldPos = AnchorActor->GetActorLocation() + AnchorWorldOffset;
	FVector2D ScreenPos;
	if (UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true))
	{
		SetPositionInViewport(ScreenPos + ScreenOffset);
	}
}

void UClcStoneInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateScreenPosition();
}
