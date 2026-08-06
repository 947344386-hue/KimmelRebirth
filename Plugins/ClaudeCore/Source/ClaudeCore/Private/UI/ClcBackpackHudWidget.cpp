// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcBackpackHudWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

UClcBackpackHudWidget::UClcBackpackHudWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UClcBackpackHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcBackpackHudWidget::SetGold(int32 InGold)
{
	if (GoldText)
	{
		GoldText->SetText(FText::AsNumber(InGold));
	}
}

void UClcBackpackHudWidget::SetStoneCount(int32 Current, int32 Max)
{
	if (StoneCountText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Cur"), Current);
		Args.Add(TEXT("Max"), Max);
		StoneCountText->SetText(FText::Format(NSLOCTEXT("ClcBackpack", "StoneCountFmt", "{Cur}/{Max}"), Args));
	}
}

void UClcBackpackHudWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BarRow"));

	UTextBlock* GoldLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldLabel"));
	GoldLabel->SetText(FText::FromString(TEXT("金币")));
	GoldLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	{
		FSlateFontInfo Font = GoldLabel->GetFont();
		Font.Size = 14;
		GoldLabel->SetFont(Font);
	}

	UTextBlock* GoldTB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldValue"));
	GoldTB->SetText(FText::FromString(TEXT("0")));
	GoldTB->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f)));
	{
		FSlateFontInfo Font = GoldTB->GetFont();
		Font.Size = 16;
		GoldTB->SetFont(Font);
	}

	UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Separator"));
	Sep->SetText(FText::FromString(TEXT("  ｜  ")));
	Sep->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	{
		FSlateFontInfo Font = Sep->GetFont();
		Font.Size = 14;
		Sep->SetFont(Font);
	}

	UTextBlock* StoneLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoneLabel"));
	StoneLabel->SetText(FText::FromString(TEXT("石头")));
	StoneLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f)));
	{
		FSlateFontInfo Font = StoneLabel->GetFont();
		Font.Size = 14;
		StoneLabel->SetFont(Font);
	}

	UTextBlock* StoneTB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StoneValue"));
	StoneTB->SetText(FText::FromString(TEXT("0/200")));
	StoneTB->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));
	{
		FSlateFontInfo Font = StoneTB->GetFont();
		Font.Size = 14;
		StoneTB->SetFont(Font);
	}

	UHorizontalBoxSlot* HSlot = nullptr;
	HSlot = HBox->AddChildToHorizontalBox(GoldLabel);
	HSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	HBox->AddChildToHorizontalBox(GoldTB);
	HBox->AddChildToHorizontalBox(Sep);
	HSlot = HBox->AddChildToHorizontalBox(StoneLabel);
	HSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	HBox->AddChildToHorizontalBox(StoneTB);

	// 置于屏幕右下角
	UCanvasPanelSlot* BarSlot = RootCanvas->AddChildToCanvas(HBox);
	BarSlot->SetAnchors(FAnchors(1.0f, 1.0f));
	BarSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	BarSlot->SetPosition(FVector2D(-40.0f, -40.0f));
	BarSlot->SetAutoSize(true);

	GoldText = GoldTB;
	StoneCountText = StoneTB;
}
