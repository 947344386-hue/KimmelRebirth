// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcToolUpgradeMenuWidget.h"
#include "UI/ClcToolUpgradeEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"

UClcToolUpgradeMenuWidget::UClcToolUpgradeMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	EntryWidgetClass = UClcToolUpgradeEntryWidget::StaticClass();
}

void UClcToolUpgradeMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcToolUpgradeMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeMenuWidget::HandlePurchaseClicked);
		PurchaseButton->OnClicked.AddDynamic(this, &UClcToolUpgradeMenuWidget::HandlePurchaseClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeMenuWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UClcToolUpgradeMenuWidget::HandleCloseClicked);
	}
}

void UClcToolUpgradeMenuWidget::NativeDestruct()
{
	if (PurchaseButton)
	{
		PurchaseButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeMenuWidget::HandlePurchaseClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeMenuWidget::HandleCloseClicked);
	}
	EntryWidgets.Reset();
	Super::NativeDestruct();
}

void UClcToolUpgradeMenuWidget::SetItems(const TArray<FClcToolUpgradeItemView>& InItems)
{
	Items = InItems;
	EntryWidgets.Reset();

	if (!ItemContainer)
	{
		return;
	}

	ItemContainer->ClearChildren();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (!EntryWidgetClass)
		{
			continue;
		}

		UClcToolUpgradeEntryWidget* Entry = CreateWidget<UClcToolUpgradeEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
		if (!Entry)
		{
			continue;
		}

		const FClcToolUpgradeItemView& View = Items[i];
		Entry->SetupEntry(View.Item, View.bOwned, View.bAffordable, i, this);
		ItemContainer->AddChild(Entry);
		EntryWidgets.Add(Entry);
	}

	// 默认选中第一项
	SelectedIndex = Items.Num() > 0 ? 0 : INDEX_NONE;
	RefreshSelectionVisuals();

	// 列表为空（全部升级已购买）→ 显示「已完成全部升级」
	if (EmptyStateText)
	{
		const bool bEmpty = EntryWidgets.Num() == 0;
		EmptyStateText->SetVisibility(bEmpty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UClcToolUpgradeMenuWidget::SelectItem(int32 Index)
{
	if (Items.Num() == 0)
	{
		return;
	}
	SelectedIndex = FMath::Clamp(Index, 0, Items.Num() - 1);
	RefreshSelectionVisuals();
}

void UClcToolUpgradeMenuWidget::MoveSelection(int32 Delta)
{
	if (Items.Num() == 0)
	{
		return;
	}
	if (SelectedIndex == INDEX_NONE)
	{
		SelectedIndex = 0;
	}
	else
	{
		SelectedIndex = (SelectedIndex + Delta + Items.Num()) % Items.Num();
	}
	RefreshSelectionVisuals();
}

void UClcToolUpgradeMenuWidget::RefreshSelectionVisuals()
{
	for (int32 i = 0; i < EntryWidgets.Num(); ++i)
	{
		if (UClcToolUpgradeEntryWidget* Entry = EntryWidgets[i].Get())
		{
			Entry->SetSelected(i == SelectedIndex);
		}
	}

	if (PurchaseButton)
	{
		const bool bHasSelection = SelectedIndex != INDEX_NONE && Items.IsValidIndex(SelectedIndex);
		bool bCanBuy = false;
		if (bHasSelection)
		{
			const FClcToolUpgradeItemView& View = Items[SelectedIndex];
			bCanBuy = !View.bOwned && View.bAffordable;
		}
		PurchaseButton->SetIsEnabled(bCanBuy);
	}
}

void UClcToolUpgradeMenuWidget::ConfirmSelected()
{
	if (SelectedIndex != INDEX_NONE && Items.IsValidIndex(SelectedIndex))
	{
		// 用原始索引（升级台 Upgrades 数组中的位置），过滤已拥有项后仍能回查正确目标
		OnPurchaseRequested.Broadcast(Items[SelectedIndex].SourceIndex);
	}
}

void UClcToolUpgradeMenuWidget::RequestClose()
{
	OnClosed.Broadcast();
}

void UClcToolUpgradeMenuWidget::HandlePurchaseClicked()
{
	ConfirmSelected();
}

void UClcToolUpgradeMenuWidget::HandleCloseClicked()
{
	RequestClose();
}

FReply UClcToolUpgradeMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		ConfirmSelected();
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Left)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Right)
	{
		MoveSelection(1);
		return FReply::Handled();
	}

	// 数字键 1~9 直接选中
	if (Key == EKeys::One)   { SelectItem(0); return FReply::Handled(); }
	if (Key == EKeys::Two)   { SelectItem(1); return FReply::Handled(); }
	if (Key == EKeys::Three) { SelectItem(2); return FReply::Handled(); }
	if (Key == EKeys::Four)  { SelectItem(3); return FReply::Handled(); }
	if (Key == EKeys::Five)  { SelectItem(4); return FReply::Handled(); }
	if (Key == EKeys::Six)   { SelectItem(5); return FReply::Handled(); }
	if (Key == EKeys::Seven) { SelectItem(6); return FReply::Handled(); }
	if (Key == EKeys::Eight) { SelectItem(7); return FReply::Handled(); }
	if (Key == EKeys::Nine)  { SelectItem(8); return FReply::Handled(); }

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UClcToolUpgradeMenuWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// ── 根：全屏 CanvasPanel ──
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// 全屏暗色背景
	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Bg->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
	{
		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Bg);
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}

	// ── 顶部标题 ──
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(NSLOCTEXT("ClcUpgrade", "Title", "工具升级"));
	TitleText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 1.0f));
	TitleText->SetJustification(ETextJustify::Center);
	{
		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(TitleText);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, 48.0f));
		CanvasSlot->SetAutoSize(true);
	}

	// ── 居中内容区：列表 + 按钮 ──
	UVerticalBox* CenterVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterVBox"));
	{
		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(CenterVBox);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetAutoSize(true);
	}

	// 升级项目列表（从上到下）
	ItemContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ItemContainer"));
	CenterVBox->AddChildToVerticalBox(ItemContainer);

	// 全部购买后的提示
	EmptyStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EmptyStateText"));
	EmptyStateText->SetText(NSLOCTEXT("ClcUpgrade", "AllCompleted", "已完成全部升级"));
	EmptyStateText->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.4f));
	EmptyStateText->SetJustification(ETextJustify::Center);
	EmptyStateText->SetVisibility(ESlateVisibility::Collapsed);
	CenterVBox->AddChildToVerticalBox(EmptyStateText);

	// 购买按钮
	PurchaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PurchaseButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PurchaseLabel"));
		Label->SetText(NSLOCTEXT("ClcUpgrade", "Purchase", "购买选中"));
		PurchaseButton->SetContent(Label);
		CenterVBox->AddChildToVerticalBox(PurchaseButton);
	}

	// ── 底部提示 ──
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	Hint->SetText(NSLOCTEXT("ClcUpgrade", "Hint", "方向键选择 | Enter 购买 | Esc 关闭"));
	Hint->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f));
	Hint->SetJustification(ETextJustify::Center);
	{
		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(Hint);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -40.0f));
		CanvasSlot->SetAutoSize(true);
	}

	// ── 右上角关闭按钮 ──
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
		Label->SetText(NSLOCTEXT("ClcUpgrade", "Close", "✕"));
		Label->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f));
		CloseButton->SetContent(Label);
		CloseButton->SetBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(CloseButton);
		CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(-32.0f, 24.0f));
		CanvasSlot->SetAutoSize(true);
	}
}
