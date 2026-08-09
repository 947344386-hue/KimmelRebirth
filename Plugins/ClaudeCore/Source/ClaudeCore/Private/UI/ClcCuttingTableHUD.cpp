// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcCuttingTableHUD.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UClcCuttingTableHUD::UClcCuttingTableHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcCuttingTableHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcCuttingTableHUD::RefreshData(const FClcCuttingTableHUDData& Data)
{
	auto SetText = [](UTextBlock* TextBlock, const FString& Value)
	{
		if (TextBlock)
		{
			TextBlock->SetText(FText::FromString(Value));
		}
	};

	SetText(DisplayNameText, Data.DisplayName);
	SetText(OriginText, Data.Origin);

	const TCHAR* Grade = TEXT("豆种");
	switch (Data.GradeValue)
	{
	case 1: Grade = TEXT("糯种"); break;
	case 2: Grade = TEXT("冰种"); break;
	case 3: Grade = TEXT("玻璃种"); break;
	default: break;
	}
	SetText(GradeText, Data.bGradeRevealed
		? FString::Printf(TEXT("种水 %s"), Grade)
		: TEXT("种水 未揭示"));
	SetText(CutCountText, FString::Printf(TEXT("已解 %d 刀"), Data.CutCount));
	SetText(RemovedVolumeText, FString::Printf(TEXT("已解体积 %.0f cm³"), Data.RemovedVolume));
	SetText(RemainingVolumeText, FString::Printf(TEXT("剩余体积 %.0f cm³"), Data.RemainingVolume));
	SetText(PositionText, FString::Printf(TEXT("石位 %+.1f / ±%.1f cm"), Data.StoneOffset, Data.MovementRange));
	SetText(BladeText, FString::Printf(TEXT("解石刀 %.0f / %.0f"), Data.BladeCurrent, Data.BladeMax));
	SetText(CutStateText, Data.bCanCut ? TEXT("可下刀") : TEXT("调整石位或修复解石刀"));
	SetText(HintsText, Data.OperationHints);
	SetText(RemainingVolumeText, FString::Printf(TEXT("剩余 %.0f cm³ | 结算 %d 金 | 估值 %d 金"),
		Data.RemainingVolume, Data.SettledGold, Data.CurrentValuation));

	if (BladeText)
	{
		const FLinearColor Color = Data.BladeDurability <= 0.0f
			? FLinearColor::Red
			: (Data.BladeDurability < 0.2f ? FLinearColor::Yellow : FLinearColor(0.3f, 0.9f, 0.5f));
		BladeText->SetColorAndOpacity(FSlateColor(Color));
	}
	if (BladeProgressBar)
	{
		const float Ratio = FMath::Clamp(Data.BladeDurability, 0.0f, 1.0f);
		BladeProgressBar->SetPercent(Ratio);
		const FLinearColor BarColor = Data.BladeDurability <= 0.0f
			? FLinearColor::Red
			: (Data.BladeDurability < 0.2f ? FLinearColor::Yellow : FLinearColor(0.3f, 0.9f, 0.5f));
		BladeProgressBar->SetFillColorAndOpacity(BarColor);
	}
	if (CutStateText)
	{
		CutStateText->SetColorAndOpacity(FSlateColor(
			Data.bCanCut ? FLinearColor(0.3f, 1.0f, 0.5f) : FLinearColor(1.0f, 0.55f, 0.2f)));
	}
}

void UClcCuttingTableHUD::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	WidgetTree->RootWidget = RootCanvas;

	auto AddCard = [&](const FName Name, const FAnchors& Anchors, const FVector2D& Alignment,
		const FVector2D& Position) -> UVerticalBox*
	{
		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
		SizeBox->SetMinDesiredWidth(280.0f);

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Card->SetPadding(FMargin(14.0f));
		Card->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.78f));
		SizeBox->SetContent(Card);

		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(SizeBox);
		CanvasSlot->SetAnchors(Anchors);
		CanvasSlot->SetAlignment(Alignment);
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetAutoSize(true);

		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Box);
		return Box;
	};

	auto AddText = [&](UVerticalBox* Box, const FName Name, int32 Size, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Box->AddChildToVerticalBox(Text);
		return Text;
	};

	UVerticalBox* Left = AddCard(TEXT("StoneCard"), FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(20.0f, 20.0f));
	DisplayNameText = AddText(Left, TEXT("DisplayNameText"), 25, FLinearColor::White);
	OriginText = AddText(Left, TEXT("OriginText"), 16, FLinearColor(0.75f, 0.8f, 0.9f));
	GradeText = AddText(Left, TEXT("GradeText"), 18, FLinearColor(0.3f, 0.95f, 0.55f));
	CutCountText = AddText(Left, TEXT("CutCountText"), 18, FLinearColor::White);
	RemovedVolumeText = AddText(Left, TEXT("RemovedVolumeText"), 16, FLinearColor(0.8f, 0.8f, 0.8f));
	RemainingVolumeText = AddText(Left, TEXT("RemainingVolumeText"), 18, FLinearColor(1.0f, 0.85f, 0.25f));

	UVerticalBox* Right = AddCard(TEXT("ToolCard"), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-20.0f, 20.0f));
	PositionText = AddText(Right, TEXT("PositionText"), 18, FLinearColor::White);
	BladeText = AddText(Right, TEXT("BladeText"), 20, FLinearColor(0.3f, 0.9f, 0.5f));

	BladeProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BladeProgressBar"));
	BladeProgressBar->SetPercent(1.0f);
	BladeProgressBar->SetFillColorAndOpacity(FLinearColor(0.3f, 0.9f, 0.5f));
	if (UVerticalBoxSlot* BarSlot = Right->AddChildToVerticalBox(BladeProgressBar))
	{
		BarSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
	}

	CutStateText = AddText(Right, TEXT("CutStateText"), 18, FLinearColor(0.3f, 1.0f, 0.5f));

	HintsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintsText"));
	FSlateFontInfo HintFont = HintsText->GetFont();
	HintFont.Size = 15;
	HintsText->SetFont(HintFont);
	HintsText->SetJustification(ETextJustify::Center);
	HintsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.85f)));
	UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(HintsText);
	HintSlot->SetAnchors(FAnchors(0.5f, 1.0f));
	HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	HintSlot->SetPosition(FVector2D(0.0f, -24.0f));
	HintSlot->SetAutoSize(true);
}
