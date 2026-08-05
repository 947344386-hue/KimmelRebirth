// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcToolUpgradeEntryWidget.h"
#include "UI/ClcToolUpgradeMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UClcToolUpgradeEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcToolUpgradeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeEntryWidget::HandleClicked);
		RowButton->OnClicked.AddDynamic(this, &UClcToolUpgradeEntryWidget::HandleClicked);
	}
}

void UClcToolUpgradeEntryWidget::NativeDestruct()
{
	if (RowButton)
	{
		RowButton->OnClicked.RemoveDynamic(this, &UClcToolUpgradeEntryWidget::HandleClicked);
	}
	OwnerMenu.Reset();
	Super::NativeDestruct();
}

void UClcToolUpgradeEntryWidget::SetupEntry(const FClcToolUpgradeItem& Item, bool bOwned, bool bAffordable,
	int32 InIndex, UClcToolUpgradeMenuWidget* InOwner)
{
	EntryIndex = InIndex;
	bEntryOwned = bOwned;
	OwnerMenu = InOwner;

	if (NameText)
	{
		NameText->SetText(FText::FromString(Item.Name));
	}
	if (DescText)
	{
		DescText->SetText(FText::FromString(Item.Description));
	}
	if (CostText)
	{
		CostText->SetText(FText::FromString(FString::Printf(TEXT("%d 金"), Item.Cost)));
	}

	ApplyVisualState();
}

void UClcToolUpgradeEntryWidget::SetSelected(bool bSelected)
{
	bEntrySelected = bSelected;
	ApplyVisualState();
}

void UClcToolUpgradeEntryWidget::ApplyVisualState()
{
	// 状态文案 + 颜色
	if (StateText)
	{
		FText Label;
		FLinearColor Color = FLinearColor::White;
		if (bEntryOwned)
		{
			Label = NSLOCTEXT("ClcUpgrade", "Owned", "已拥有");
			Color = FLinearColor(0.5f, 0.5f, 0.5f);
		}
		else if (bEntrySelected)
		{
			Label = NSLOCTEXT("ClcUpgrade", "Affordable", "可购买");
			Color = FLinearColor(0.2f, 1.0f, 1.0f);
		}
		StateText->SetText(Label);
		StateText->SetColorAndOpacity(Color);
	}

	// 行背景：选中高亮
	if (RowButton)
	{
		RowButton->SetBackgroundColor(bEntrySelected
			? FLinearColor(0.0f, 0.35f, 0.45f, 0.6f)
			: FLinearColor(0.1f, 0.1f, 0.12f, 0.4f));
	}
}

void UClcToolUpgradeEntryWidget::HandleClicked()
{
	if (UClcToolUpgradeMenuWidget* Menu = OwnerMenu.Get())
	{
		Menu->SelectItem(EntryIndex);
	}
}

void UClcToolUpgradeEntryWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RowButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RowButton"));
	RowButton->SetBackgroundColor(FLinearColor(0.1f, 0.1f, 0.12f, 0.4f));
	WidgetTree->RootWidget = RowButton;

	// 条目内容：名称行 + 描述行
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EntryRows"));
	RowButton->SetContent(Rows);
	if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(RowButton->GetContentSlot()))
	{
		BtnSlot->SetPadding(FMargin(16.0f, 8.0f));
	}

	// 第一行：名称 | 价格 | 状态
	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopRow"));
	Rows->AddChildToVerticalBox(TopRow);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetColorAndOpacity(FLinearColor::White);
	NameText->SetJustification(ETextJustify::Left);
	TopRow->AddChildToHorizontalBox(NameText);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
	CostText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.3f));
	CostText->SetJustification(ETextJustify::Right);
	TopRow->AddChildToHorizontalBox(CostText);

	StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StateText"));
	StateText->SetJustification(ETextJustify::Right);
	TopRow->AddChildToHorizontalBox(StateText);

	// 第二行：描述
	DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescText"));
	DescText->SetColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f));
	Rows->AddChildToVerticalBox(DescText);
}
