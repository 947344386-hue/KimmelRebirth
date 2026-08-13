// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcVendorHUD.h"
#include "UI/ClcWidgetPalette.h"
#include "Actors/ClcStoneVendor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

UClcVendorHUD::UClcVendorHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcVendorHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcVendorHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (SellButton)
	{
		SellButton->OnClicked.RemoveDynamic(this, &UClcVendorHUD::HandleSellClicked);
		SellButton->OnClicked.AddDynamic(this, &UClcVendorHUD::HandleSellClicked);
	}
}

void UClcVendorHUD::NativeDestruct()
{
	if (SellButton)
	{
		SellButton->OnClicked.RemoveDynamic(this, &UClcVendorHUD::HandleSellClicked);
	}
	OwningVendor.Reset();
	Super::NativeDestruct();
}

void UClcVendorHUD::HandleSellClicked()
{
	RequestSellFromUI();
}

void UClcVendorHUD::RequestSellFromUI()
{
	if (OwningVendor.IsValid())
	{
		OwningVendor->RequestSell();
	}
}

void UClcVendorHUD::RefreshData(const FClcVendorHUDData& Data)
{
	auto SetText = [](UTextBlock* Tb, const FString& S)
	{
		if (Tb) Tb->SetText(FText::FromString(S));
	};

	// ── 自适应定位：背包开→上方，关→居中 ──
	UpdateCardPositions(Data.bBackpackOpen);

	// ── 左卡：石头信息 ──
	SetText(DisplayNameText, Data.DisplayName);
	SetText(OriginText, Data.Origin);
	{
		const TCHAR* GradeRaw = TEXT("豆种");
		switch (Data.GradeValue)
		{
		case 1: GradeRaw = TEXT("糯种"); break;
		case 2: GradeRaw = TEXT("冰种"); break;
		case 3: GradeRaw = TEXT("玻璃种"); break;
		default: break;
		}
		SetText(GradeText, Data.bGradeRevealed ? FString(GradeRaw) : (TEXT("皮壳：") + Data.ShellName));
	}
	SetText(OpenedRatioText, FString::Printf(TEXT("擦石 %.0f%%"), Data.OpenedRatio * 100.f));
	SetText(GreenAreaText, FString::Printf(TEXT("绿 %.1f"), Data.GreenArea));
	SetText(BlackAreaText, FString::Printf(TEXT("杂 %.1f"), Data.BlackArea));

	// ── 右卡：价格 & 操作 ──
	SetText(SalePriceText, FString::Printf(TEXT("%d"), Data.SalePrice));
	{
		const TCHAR* Arrow = Data.ValuationTrend > 0 ? TEXT("↑") :
			(Data.ValuationTrend < 0 ? TEXT("↓") : TEXT("→"));
		SetText(ProfitText, FString::Printf(TEXT("%s %s%d"),
			Arrow, Data.ProfitAmount >= 0 ? TEXT("+") : TEXT(""), Data.ProfitAmount));
	}
	SetText(GoldText, FString::Printf(TEXT("钱包 %d"), Data.GoldBalance));

	// ── 底部提示 ──
	SetText(HintsText, Data.OperationHints);

	if (SellButton) SellButton->SetIsEnabled(Data.bCanSell);

	// ── NPC 台词 ──
	if (NpcDialogBox)
	{
		if (!Data.NpcLine.IsEmpty())
		{
			if (NpcLineText)
			{
				NpcLineText->SetText(FText::FromString(Data.NpcLine));
			}
			NpcDialogBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			NpcDialogBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UClcVendorHUD::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// ── 根 CanvasPanel ──
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// ============================================================
	// 左卡 —— 石头信息（260px）
	// ============================================================
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LeftBox"));
		Box->SetMinDesiredWidth(260.0f); // 最小 260，名字更长时卡片自动变宽

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LeftCard"));
		Card->SetPadding(FMargin(12.0f));
		Card->SetBrushColor(FClcWidgetPalette::CardDark(0.65f));
		Box->SetContent(Card);

		LeftCardSlot = RootCanvas->AddChildToCanvas(Box);
		LeftCardSlot->SetAnchors(FAnchors(0.0f, 0.5f));
		LeftCardSlot->SetAlignment(FVector2D(0.0f, 0.5f));
		LeftCardSlot->SetPosition(FVector2D(20.0f, 0.0f));
		LeftCardSlot->SetAutoSize(true);

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftLayout"));
		Card->SetContent(VBox);

		// 石头名
		DisplayNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DisplayNameText"));
		{
			FSlateFontInfo Font = DisplayNameText->GetFont();
			Font.Size = 26;
			DisplayNameText->SetFont(Font);
		}
		VBox->AddChildToVerticalBox(DisplayNameText);

		OriginText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OriginText"));
		VBox->AddChildToVerticalBox(OriginText);

		GradeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GradeText"));
		VBox->AddChildToVerticalBox(GradeText);

		// 分隔
		{
			UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LeftSep"));
			Sep->SetText(FText::FromString(TEXT("──────")));
			Sep->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));
			VBox->AddChildToVerticalBox(Sep);
		}

		OpenedRatioText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OpenedRatioText"));
		VBox->AddChildToVerticalBox(OpenedRatioText);

		GreenAreaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GreenAreaText"));
		GreenAreaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.9f, 0.3f)));
		VBox->AddChildToVerticalBox(GreenAreaText);

		BlackAreaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BlackAreaText"));
		BlackAreaText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
		VBox->AddChildToVerticalBox(BlackAreaText);
	}

	// ============================================================
	// 右卡 —— 价格 & 操作（240px）
	// ============================================================
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RightBox"));
		Box->SetMinDesiredWidth(240.0f); // 最小 240，价格数字更长时卡片自动变宽

		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RightCard"));
		Card->SetPadding(FMargin(12.0f));
		Card->SetBrushColor(FClcWidgetPalette::CardDark(0.65f));
		Box->SetContent(Card);

		RightCardSlot = RootCanvas->AddChildToCanvas(Box);
		RightCardSlot->SetAnchors(FAnchors(1.0f, 0.5f));
		RightCardSlot->SetAlignment(FVector2D(1.0f, 0.5f));
		RightCardSlot->SetPosition(FVector2D(-20.0f, 0.0f));
		RightCardSlot->SetAutoSize(true);

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightLayout"));
		Card->SetContent(VBox);

		// 回收价
		SalePriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SalePriceText"));
		{
			FSlateFontInfo Font = SalePriceText->GetFont();
			Font.Size = 40;
			SalePriceText->SetFont(Font);
			SalePriceText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f)));
			SalePriceText->SetJustification(ETextJustify::Center);
		}
		VBox->AddChildToVerticalBox(SalePriceText);

		// 盈亏
		ProfitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProfitText"));
		{
			FSlateFontInfo Font = ProfitText->GetFont();
			Font.Size = 20;
			ProfitText->SetFont(Font);
			ProfitText->SetJustification(ETextJustify::Center);
		}
		VBox->AddChildToVerticalBox(ProfitText);

		// 分隔
		{
			UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RightSep"));
			Sep->SetText(FText::FromString(TEXT("──────")));
			Sep->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));
			Sep->SetJustification(ETextJustify::Center);
			VBox->AddChildToVerticalBox(Sep);
		}

		// 钱包
		GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldText"));
		{
			FSlateFontInfo Font = GoldText->GetFont();
			Font.Size = 18;
			GoldText->SetFont(Font);
			GoldText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f)));
			GoldText->SetJustification(ETextJustify::Center);
		}
		VBox->AddChildToVerticalBox(GoldText);

		// 售出按钮
		SellButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellButton"));
		{
			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SellBtnLabel"));
			BtnLabel->SetText(FText::FromString(TEXT("售出 (Enter)")));
			BtnLabel->SetJustification(ETextJustify::Center);
			{
				FSlateFontInfo Font = BtnLabel->GetFont();
				Font.Size = 20;
				BtnLabel->SetFont(Font);
			}
			SellButton->SetContent(BtnLabel);
		}
		VBox->AddChildToVerticalBox(SellButton);
	}

	// ============================================================
	// 底部操作提示
	// ============================================================
	{
		HintsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintsText"));
		{
			FSlateFontInfo Font = HintsText->GetFont();
			Font.Size = 10;
			HintsText->SetFont(Font);
			HintsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)));
		}
		HintsSlot = RootCanvas->AddChildToCanvas(HintsText);
		HintsSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		HintsSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		HintsSlot->SetPosition(FVector2D(0.0f, -16.0f));
		HintsSlot->SetAutoSize(true);
	}

	// ============================================================
	// NPC 对话气泡（底部居中，RPG 风格；不说话时整组折叠）
	// ============================================================
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NpcBox"));
		Box->SetWidthOverride(600.0f);

		NpcDialogBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NpcDialogBox"));
		NpcDialogBox->SetPadding(FMargin(20.0f, 14.0f));
		NpcDialogBox->SetBrushColor(FClcWidgetPalette::CardDark(0.92f));

		NpcLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NpcLineText"));
		{
			FSlateFontInfo Font = NpcLineText->GetFont();
			Font.Size = 18;
			NpcLineText->SetFont(Font);
			NpcLineText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.82f)));
			NpcLineText->SetJustification(ETextJustify::Center);
			NpcLineText->SetAutoWrapText(true);
		}
		NpcDialogBox->SetContent(NpcLineText);
		Box->SetContent(NpcDialogBox);

		UCanvasPanelSlot* NpcSlot = RootCanvas->AddChildToCanvas(Box);
		NpcSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		NpcSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		NpcSlot->SetPosition(FVector2D(0.0f, -80.0f));
		NpcSlot->SetAutoSize(true);

		NpcDialogBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UClcVendorHUD::UpdateCardPositions(bool bBackpackOpen)
{
	// 背包打开 → 卡片移到上方（给背包让空间）
	// 背包关闭 → 卡片回到居中位置
	const float AnchorY = bBackpackOpen ? 0.0f : 0.5f;
	const float AlignY  = bBackpackOpen ? 0.0f : 0.5f;
	const float PosY    = bBackpackOpen ? 80.0f : 0.0f;  // 上方留 80px 呼吸空间

	if (LeftCardSlot)
	{
		LeftCardSlot->SetAnchors(FAnchors(0.0f, AnchorY));
		LeftCardSlot->SetAlignment(FVector2D(0.0f, AlignY));
		LeftCardSlot->SetPosition(FVector2D(20.0f, PosY));
	}
	if (RightCardSlot)
	{
		RightCardSlot->SetAnchors(FAnchors(1.0f, AnchorY));
		RightCardSlot->SetAlignment(FVector2D(1.0f, AlignY));
		RightCardSlot->SetPosition(FVector2D(-20.0f, PosY));
	}
}