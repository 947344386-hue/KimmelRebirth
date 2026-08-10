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

	if (CutProgressBar)
	{
		CutProgressBar->SetPercent(FMath::Clamp(Data.CutProgress, 0.0f, 1.0f));
		// 只改填充率不改颜色——百分比越高越接近完成，进度条越长
	}
	SetText(PositionText, FString::Printf(TEXT("石位 %+.1f / ±%.1f cm"), Data.StoneOffset, Data.MovementRange));
	// CutStateText 由下方四态 switch 统一覆盖
	SetText(HintsText, Data.OperationHints);

	// 解石收益（利润 = 累计结算 - 购入价）；负数表示亏钱
	{
		const int32 Profit = Data.SettledGold - Data.PurchasePrice;
		const bool bProfit = Profit > 0;
		const FString Arrow = bProfit ? TEXT("▲") : TEXT("▼");
		const FString Sign  = bProfit ? TEXT("+") : TEXT("");
		SetText(ValuationText, FString::Printf(TEXT("解石收益 %s%s%d"), *Sign, *Arrow, Profit));
		if (ValuationText)
		{
			ValuationText->SetColorAndOpacity(FSlateColor(
				bProfit ? FLinearColor(1.0f, 0.35f, 0.3f) : FLinearColor(0.3f, 0.95f, 0.5f)));
		}
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

	// 切块尺寸预判四态（右上角实时反馈，帮助玩家判断下刀是否合理）
	FString CutStateString;
	FLinearColor CutStateColor;
	if (Data.bCanSellRemaining)
	{
		CutStateString = FString::Printf(TEXT("可出售剩余主体 +%d 金"), Data.RemainingSellPrice);
		CutStateColor = FLinearColor(0.3f, 0.9f, 1.0f); // 蓝色（区别于标准切割的绿色）
	}
	else switch (Data.CutSizeState)
	{
	case EClcCutSizeState::Undersized:
		CutStateString = FString::Printf(TEXT("切块过小 %.0f%%"), Data.CutSizeRatio * 100.0f);
		CutStateColor = FLinearColor(1.0f, 0.85f, 0.25f); // 黄
		break;
	case EClcCutSizeState::Standard:
		CutStateString = FString::Printf(TEXT("切块尺寸标准 %.0f%%"), Data.CutSizeRatio * 100.0f);
		CutStateColor = FLinearColor(0.3f, 1.0f, 0.5f); // 绿
		break;
	case EClcCutSizeState::Oversized:
		CutStateString = FString::Printf(TEXT("切块过大 %.0f%%"), Data.CutSizeRatio * 100.0f);
		CutStateColor = FLinearColor(1.0f, 0.55f, 0.2f); // 橙
		break;
	default: // CannotCut
		CutStateString = TEXT("无法下刀");
		CutStateColor = FLinearColor(1.0f, 0.4f, 0.2f); // 红
		break;
	}
	SetText(CutStateText, CutStateString);
	if (CutStateText)
	{
		CutStateText->SetColorAndOpacity(FSlateColor(CutStateColor));
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

	CutProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CutProgressBar"));
	CutProgressBar->SetPercent(0.0f);
	CutProgressBar->SetFillColorAndOpacity(FLinearColor(0.3f, 0.9f, 0.55f));
	if (UVerticalBoxSlot* CutBarSlot = Left->AddChildToVerticalBox(CutProgressBar))
	{
		CutBarSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	}

	ValuationText = AddText(Left, TEXT("ValuationText"), 18, FLinearColor(1.0f, 0.85f, 0.25f));

	UVerticalBox* Right = AddCard(TEXT("ToolCard"), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-20.0f, 20.0f));
	PositionText = AddText(Right, TEXT("PositionText"), 18, FLinearColor::White);

	AddText(Right, TEXT("BladeLabelText"), 14, FLinearColor(0.7f, 0.75f, 0.85f))->SetText(FText::FromString(TEXT("刀片耐久")));

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
