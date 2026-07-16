// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcMerchantBubbleWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UClcMerchantBubbleWidget::SetAnchor(AActor* InMerchant, const FVector& InWorldOffset)
{
	AnchorMerchant = InMerchant;
	AnchorWorldOffset = InWorldOffset;
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
	if (!AnchorMerchant.IsValid())
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	const FVector WorldPos = AnchorMerchant->GetActorLocation() + AnchorWorldOffset;
	FVector2D ScreenPos;
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true);

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);

	const FVector2D FinalPos = ScreenPos + ScreenOffset;
	const bool bOnScreen = bProjected && ViewportX > 0 && ViewportY > 0
		&& FinalPos.X >= 0.0f && FinalPos.X <= static_cast<float>(ViewportX)
		&& FinalPos.Y >= 0.0f && FinalPos.Y <= static_cast<float>(ViewportY);

	if (!bOnScreen)
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// WBP 默认使用 SelfHitTestInvisible；恢复该状态避免气泡拦截鼠标输入。
	if (GetVisibility() == ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	SetPositionInViewport(FinalPos);
}
