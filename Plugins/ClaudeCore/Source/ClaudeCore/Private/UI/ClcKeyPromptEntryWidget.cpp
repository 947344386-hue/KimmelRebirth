// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcKeyPromptEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

UClcKeyPromptEntryWidget::UClcKeyPromptEntryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UClcKeyPromptEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcKeyPromptEntryWidget::SetPrompt(const FKey& Key, const FText& Label)
{
	if (KeyText)
	{
		KeyText->SetText(FText::FromString(Key.ToString()));
	}
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
}

void UClcKeyPromptEntryWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EntryRoot"));
	WidgetTree->RootWidget = HBox;

	UTextBlock* KeyTB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyText"));
	KeyTB->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f)));
	FSlateFontInfo KeyFont = KeyTB->GetFont();
	KeyFont.Size = 18;
	KeyTB->SetFont(KeyFont);

	UTextBlock* LabelTB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LabelText"));
	LabelTB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo LabelFont = LabelTB->GetFont();
	LabelFont.Size = 18;
	LabelTB->SetFont(LabelFont);

	UHorizontalBoxSlot* KeySlot = HBox->AddChildToHorizontalBox(KeyTB);
	KeySlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	HBox->AddChildToHorizontalBox(LabelTB);

	KeyText = KeyTB;
	LabelText = LabelTB;
}
