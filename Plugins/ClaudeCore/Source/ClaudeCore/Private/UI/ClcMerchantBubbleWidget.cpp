// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcMerchantBubbleWidget.h"
#include "Components/TextBlock.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UClcMerchantBubbleWidget::SetAnchor(USceneComponent* InAnchorComponent, const FVector& InLocalOffset)
{
	AnchorComponent = InAnchorComponent;
	AnchorLocalOffset = InLocalOffset;
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UClcMerchantBubbleWidget::SetSimulatedPerspective(const FClcMerchantUISimulatedPerspectiveSettings& InSettings)
{
	SimulatedPerspective = InSettings;
}

void UClcMerchantBubbleWidget::SetBubbleText(const FText& Text)
{
	if (BubbleTextBlock)
	{
		BubbleTextBlock->SetText(Text);
	}
}

void UClcMerchantBubbleWidget::SetSecondaryText(const FText& Text)
{
	if (!SecondaryTextBlock) return;

	if (Text.IsEmpty())
	{
		SecondaryTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SecondaryTextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SecondaryTextBlock->SetText(Text);
	}
}

void UClcMerchantBubbleWidget::UpdateScreenPosition()
{
	if (!AnchorComponent.IsValid())
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	const FVector WorldPos = AnchorComponent->GetComponentTransform().TransformPositionNoScale(AnchorLocalOffset);
	FVector2D ScreenPos;
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true);

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);

	const bool bOnScreen = bProjected && ViewportX > 0 && ViewportY > 0
		&& ScreenPos.X >= 0.0f && ScreenPos.X <= static_cast<float>(ViewportX)
		&& ScreenPos.Y >= 0.0f && ScreenPos.Y <= static_cast<float>(ViewportY);

	if (!bOnScreen)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	float Scale = 1.0f;
	if (SimulatedPerspective.bEnabled
		&& SimulatedPerspective.FarDistance > SimulatedPerspective.NearDistance)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		Scale = FMath::GetMappedRangeValueClamped(
			FVector2D(SimulatedPerspective.NearDistance, SimulatedPerspective.FarDistance),
			FVector2D(SimulatedPerspective.NearScale, SimulatedPerspective.FarScale),
			FVector::Distance(CameraLocation, WorldPos));
	}
	SetRenderScale(FVector2D(Scale));

	// WBP 默认使用 SelfHitTestInvisible；恢复该状态避免气泡拦截鼠标输入。
	if (GetVisibility() == ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	SetPositionInViewport(ScreenPos);
}
