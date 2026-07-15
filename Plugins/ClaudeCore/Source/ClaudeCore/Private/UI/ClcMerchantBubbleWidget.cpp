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

void UClcMerchantBubbleWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!AnchorMerchant.IsValid())
	{
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	const FVector WorldPos = AnchorMerchant->GetActorLocation() + AnchorWorldOffset;
	FVector2D ScreenPos;
	UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true);
	SetPositionInViewport(ScreenPos + ScreenOffset);
}
