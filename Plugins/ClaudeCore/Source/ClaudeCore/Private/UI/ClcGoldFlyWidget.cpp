// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcGoldFlyWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

static float RandOffset() { return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) - 0.5f) * 2.0f; }

UClcGoldFlyWidget::UClcGoldFlyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcGoldFlyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcGoldFlyWidget::StartFlight(const FVector2D& ScreenFrom, int32 GoldAmount)
{
	Amount = GoldAmount;
	From = ScreenFrom;

	// 目标 = 右下角金币条位置
	FVector2D ViewportSize(1280, 720);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	To = FVector2D(ViewportSize.X - 60.0f, ViewportSize.Y - 60.0f);

	if (GoldLabel)
	{
		GoldLabel->SetText(FText::FromString(FString::Printf(TEXT("+%d"), GoldAmount)));
		GoldLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f)));
	}

	Elapsed = 0.0f;
	bFlying = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UClcGoldFlyWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bFlying) return;

	Elapsed += InDeltaTime;
	const float T = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);

	// 抛物线 + ease-out
	const float Arc = FMath::Sin(T * PI) * 80.0f * (1.0f - T);
	const FVector2D Pos = FMath::Lerp(From, To, T * T * (3.0f - 2.0f * T));
	SetRenderTranslation(FVector2D(Pos.X, Pos.Y - Arc));

	// 最后 0.2s 淡出
	const float FadeStart = 0.6f;
	const float Opacity = T > FadeStart ? 1.0f - (T - FadeStart) / (1.0f - FadeStart) : 1.0f;
	SetRenderOpacity(FMath::Max(0.0f, Opacity));

	if (Elapsed >= Duration)
	{
		bFlying = false;
		RemoveFromParent();
	}
}

void UClcGoldFlyWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox"));
	Box->SetWidthOverride(120.0f);
	Box->SetHeightOverride(40.0f);

	GoldLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldLabel"));
	GoldLabel->SetJustification(ETextJustify::Center);
	{
		FSlateFontInfo Font = GoldLabel->GetFont();
		Font.Size = 28;
		GoldLabel->SetFont(Font);
	}
	Box->AddChild(GoldLabel);

	UCanvasPanelSlot* GfSlot = Root->AddChildToCanvas(Box);
	GfSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	GfSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	GfSlot->SetAutoSize(true);
}