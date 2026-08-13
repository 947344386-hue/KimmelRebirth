// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcHaggleWidget.h"
#include "UI/ClcWidgetPalette.h"
#include "Components/ClcHaggleComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

UClcHaggleWidget::UClcHaggleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcHaggleWidget::SetOwningComponent(UClcHaggleComponent* InComp)
{
	OwningComponent = InComp;
}

void UClcHaggleWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcHaggleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AcceptButton)
	{
		AcceptButton->OnClicked.RemoveDynamic(this, &UClcHaggleWidget::HandleAcceptClicked);
		AcceptButton->OnClicked.AddDynamic(this, &UClcHaggleWidget::HandleAcceptClicked);
	}
}

void UClcHaggleWidget::NativeDestruct()
{
	if (AcceptButton)
	{
		AcceptButton->OnClicked.RemoveDynamic(this, &UClcHaggleWidget::HandleAcceptClicked);
	}
	OwningComponent.Reset();
	Super::NativeDestruct();
}

void UClcHaggleWidget::HandleAcceptClicked()
{
	if (OwningComponent.IsValid())
	{
		OwningComponent->ChooseAccept();
	}
}

const TCHAR* UClcHaggleWidget::GlyphFor(uint8 KeyIndex)
{
	switch (KeyIndex)
	{
	case 0: return TEXT("↑"); // ↑ W
	case 1: return TEXT("←"); // ← A
	case 2: return TEXT("↓"); // ↓ S
	case 3: return TEXT("→"); // → D
	default: return TEXT("?");
	}
}

UTextBlock* UClcHaggleWidget::MakeTextBlock(const FString& Name)
{
	if (WidgetTree)
	{
		return WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*Name));
	}
	return NewObject<UTextBlock>(this, UTextBlock::StaticClass(), FName(*Name));
}

void UClcHaggleWidget::ShowSelectionWidgets(bool bShow)
{
	const ESlateVisibility Vis = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (AcceptButton) AcceptButton->SetVisibility(Vis);
	if (TierContainer) TierContainer->SetVisibility(Vis);
}

void UClcHaggleWidget::ShowPlayingWidgets(bool bShow)
{
	const ESlateVisibility Vis = bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (SequenceContainer) SequenceContainer->SetVisibility(Vis);
	if (KeyTimerBar) KeyTimerBar->SetVisibility(Vis);
}

void UClcHaggleWidget::SetupSelection(int32 InReferencePrice, const TArray<FClcHaggleTier>& Tiers, const UClcHaggleConfig* Config)
{
	// 参考价
	if (ReferencePriceText)
	{
		ReferencePriceText->SetText(FText::FromString(FString::Printf(TEXT("%d 金"), InReferencePrice)));
	}

	// NPC 报价
	if (NpcLineText && Config)
	{
		TArray<FStringFormatArg> Args;
		Args.Add(FStringFormatArg(InReferencePrice));
		NpcLineText->SetText(FText::FromString(FString::Format(*Config->OfferLineTemplate.ToString(), Args)));
	}

	// 档位提示（按数字键选择；纯展示，非交互）
	if (TierContainer)
	{
		TierContainer->ClearChildren();
		for (int32 i = 0; i < Tiers.Num(); ++i)
		{
			UTextBlock* TierText = MakeTextBlock(FString::Printf(TEXT("Tier_%d"), i));
			{
				FSlateFontInfo Font = TierText->GetFont();
				Font.Size = 20;
				TierText->SetFont(Font);
			}
			TierText->SetText(FText::FromString(FString::Printf(TEXT("按 %d 加价 %s  (%d键)"),
				i + 1, *Tiers[i].Label.ToString(), Tiers[i].SequenceLength)));
			TierContainer->AddChild(TierText);
		}
	}

	if (ResultText) ResultText->SetVisibility(ESlateVisibility::Collapsed);

	ShowSelectionWidgets(true);
	ShowPlayingWidgets(false);
}

void UClcHaggleWidget::StartSequence(const TArray<uint8>& Sequence)
{
	// 构造方向箭头
	if (SequenceContainer)
	{
		SequenceContainer->ClearChildren();
		SequenceArrows.Reset();

		for (int32 i = 0; i < Sequence.Num(); ++i)
		{
			UTextBlock* Arrow = MakeTextBlock(FString::Printf(TEXT("Arrow_%d"), i));
			{
				FSlateFontInfo Font = Arrow->GetFont();
				Font.Size = 46;
				Arrow->SetFont(Font);
			}
			Arrow->SetText(FText::FromString(GlyphFor(Sequence[i])));
			Arrow->SetJustification(ETextJustify::Center);
			SequenceContainer->AddChild(Arrow);
			SequenceArrows.Add(Arrow);
		}
	}

	if (NpcLineText)
	{
		NpcLineText->SetText(FText::FromString(TEXT("快！按顺序敲出方向")));
	}

	UpdatePlaying(0, 1.0f);

	if (ResultText) ResultText->SetVisibility(ESlateVisibility::Collapsed);

	ShowSelectionWidgets(false);
	ShowPlayingWidgets(true);
}

void UClcHaggleWidget::UpdatePlaying(int32 CurrentIndex, float TimerFraction)
{
	const FLinearColor ActiveColor(1.0f, 0.85f, 0.2f);
	const FLinearColor DoneColor(0.3f, 0.7f, 0.3f);
	const FLinearColor IdleColor(0.5f, 0.5f, 0.5f);

	for (int32 i = 0; i < SequenceArrows.Num(); ++i)
	{
		if (UTextBlock* Arrow = SequenceArrows[i])
		{
			FLinearColor C = IdleColor;
			if (i < CurrentIndex) C = DoneColor;
			else if (i == CurrentIndex) C = ActiveColor;
			Arrow->SetColorAndOpacity(FSlateColor(C));
		}
	}

	if (KeyTimerBar)
	{
		KeyTimerBar->SetPercent(FMath::Clamp(TimerFraction, 0.0f, 1.0f));
	}
}

void UClcHaggleWidget::ShowResult(const FText& Line, bool bSuccess)
{
	if (ResultText)
	{
		ResultText->SetText(Line);
		ResultText->SetColorAndOpacity(FSlateColor(bSuccess
			? FLinearColor(0.3f, 1.0f, 0.3f)
			: FLinearColor(1.0f, 0.35f, 0.35f)));
		ResultText->SetVisibility(ESlateVisibility::Visible);
	}

	ShowSelectionWidgets(false);
	ShowPlayingWidgets(false);
}

void UClcHaggleWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// 居中卡片
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HaggleBox"));
	// 不设固定宽度——卡面随内容（箭头数量）自然变宽，始终居中

	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HaggleCard"));
	Card->SetPadding(FMargin(20.0f));
	Card->SetBrushColor(FClcWidgetPalette::PanelDark(0.8f));
	Box->SetContent(Card);

	UCanvasPanelSlot* CardSlot = RootCanvas->AddChildToCanvas(Box);
	CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CardSlot->SetPosition(FVector2D(0.0f, -60.0f));
	CardSlot->SetAutoSize(true);

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HaggleLayout"));
	Card->SetContent(VBox);

	// NPC 台词
	NpcLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NpcLineText"));
	{
		FSlateFontInfo Font = NpcLineText->GetFont();
		Font.Size = 20;
		NpcLineText->SetFont(Font);
	}
	NpcLineText->SetJustification(ETextJustify::Center);
	VBox->AddChildToVerticalBox(NpcLineText);

	// 参考价（大号金色）
	ReferencePriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReferencePriceText"));
	{
		FSlateFontInfo Font = ReferencePriceText->GetFont();
		Font.Size = 40;
		ReferencePriceText->SetFont(Font);
		ReferencePriceText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f)));
		ReferencePriceText->SetJustification(ETextJustify::Center);
	}
	VBox->AddChildToVerticalBox(ReferencePriceText);

	// 结果文案（默认隐藏）
	ResultText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResultText"));
	{
		FSlateFontInfo Font = ResultText->GetFont();
		Font.Size = 24;
		ResultText->SetFont(Font);
		ResultText->SetJustification(ETextJustify::Center);
	}
	ResultText->SetVisibility(ESlateVisibility::Collapsed);
	VBox->AddChildToVerticalBox(ResultText);

	// 直接出手按钮
	AcceptButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AcceptButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AcceptLabel"));
		Label->SetText(FText::FromString(TEXT("直接出手 (空格)")));
		Label->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = 20;
		Label->SetFont(Font);
		AcceptButton->SetContent(Label);
	}
	VBox->AddChildToVerticalBox(AcceptButton);

	// 档位提示容器
	TierContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TierContainer"));
	VBox->AddChildToVerticalBox(TierContainer);

	// 方向序列容器（水平居中，不换行，不缩小——容器随箭头数量变宽，卡面整体居中）
	SequenceContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SequenceContainer"));
	{
		if (UVerticalBoxSlot* VSlot = VBox->AddChildToVerticalBox(SequenceContainer))
		{
			VSlot->SetHorizontalAlignment(HAlign_Center);
			VSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
		}
	}

	// 每键倒计时条
	KeyTimerBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("KeyTimerBar"));
	KeyTimerBar->SetPercent(1.0f);
	VBox->AddChildToVerticalBox(KeyTimerBar);
}
