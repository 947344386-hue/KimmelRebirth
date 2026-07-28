// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcKeyPromptWidget.h"
#include "UI/ClcKeyPromptEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"

UClcKeyPromptWidget::UClcKeyPromptWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UClcKeyPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcKeyPromptWidget::Refresh(const TArray<FClcKeyPrompt>& Prompts)
{
	if (!PromptContainer)
	{
		return;
	}

	PromptContainer->ClearChildren();

	TSubclassOf<UClcKeyPromptEntryWidget> EntryClass = EntryWidgetClass;
	if (!EntryClass)
	{
		EntryClass = LoadClass<UClcKeyPromptEntryWidget>(nullptr,
			TEXT("/Game/JadeBetting/UI/WBP_KeyPromptEntry.WBP_KeyPromptEntry_C"));
	}
	if (!EntryClass)
	{
		EntryClass = UClcKeyPromptEntryWidget::StaticClass();
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	for (const FClcKeyPrompt& Prompt : Prompts)
	{
		UClcKeyPromptEntryWidget* Entry = CreateWidget<UClcKeyPromptEntryWidget>(PC, EntryClass);
		if (Entry)
		{
			PromptContainer->AddChild(Entry);
			Entry->SetPrompt(Prompt.Key, Prompt.Label);
		}
	}
}

void UClcKeyPromptWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PromptContainer"));
	UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(VBox);
	CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f));
	CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	CanvasSlot->SetPosition(FVector2D(40.0f, -40.0f));
	CanvasSlot->SetAutoSize(true);
	PromptContainer = VBox;
}
