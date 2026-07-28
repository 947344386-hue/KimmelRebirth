// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/Teleport/UI/ClcTeleportMenuWidget.h"
#include "Tools/Teleport/UI/ClcTeleportEntryWidget.h"
#include "Tools/Teleport/ClcTeleportPoint.h"
#include "Tools/Teleport/ClcTeleportSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"

UClcTeleportMenuWidget::UClcTeleportMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	EntryWidgetClass = UClcTeleportEntryWidget::StaticClass();
}

void UClcTeleportMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcTeleportMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UClcTeleportMenuWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UClcTeleportMenuWidget::HandleCloseClicked);
	}
}

void UClcTeleportMenuWidget::NativeDestruct()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UClcTeleportMenuWidget::HandleCloseClicked);
	}
	TeleportSubsystem.Reset();
	Super::NativeDestruct();
}

void UClcTeleportMenuWidget::InitializeMenu(UClcTeleportSubsystem* InSubsystem, const FText& InTitle,
	const TArray<AClcTeleportPoint*>& InDestinations)
{
	TeleportSubsystem = InSubsystem;

	if (TitleText)
	{
		TitleText->SetText(InTitle);
	}
	if (!DestinationContainer)
	{
		return;
	}

	DestinationContainer->ClearChildren();
	for (AClcTeleportPoint* Destination : InDestinations)
	{
		if (!IsValid(Destination) || !EntryWidgetClass)
		{
			continue;
		}

		UClcTeleportEntryWidget* Entry = CreateWidget<UClcTeleportEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
		if (Entry)
		{
			Entry->InitializeEntry(Destination, this);
			DestinationContainer->AddChild(Entry);
		}
	}

	if (EmptyStateText)
	{
		EmptyStateText->SetVisibility(DestinationContainer->GetChildrenCount() == 0
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UClcTeleportMenuWidget::SelectDestination(AClcTeleportPoint* Destination)
{
	if (UClcTeleportSubsystem* Subsystem = TeleportSubsystem.Get())
	{
		Subsystem->RequestTeleport(Destination);
	}
}

FReply UClcTeleportMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::T || Key == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UClcTeleportMenuWidget::HandleCloseClicked()
{
	RequestClose();
}

void UClcTeleportMenuWidget::RequestClose()
{
	if (UClcTeleportSubsystem* Subsystem = TeleportSubsystem.Get())
	{
		Subsystem->CloseMenu();
	}
}

void UClcTeleportMenuWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DefaultRoot"));
	RootBorder->SetPadding(FMargin(48.0f));
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DefaultLayout"));
	RootBorder->SetContent(Layout);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	Layout->AddChildToVerticalBox(TitleText);

	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DestinationScroll"));
	Layout->AddChildToVerticalBox(ScrollBox);

	DestinationContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DestinationContainer"));
	ScrollBox->AddChild(DestinationContainer);

	EmptyStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EmptyStateText"));
	EmptyStateText->SetText(NSLOCTEXT("ClcTeleport", "EmptyDestinations", "暂无可用传送点"));
	EmptyStateText->SetJustification(ETextJustify::Center);
	Layout->AddChildToVerticalBox(EmptyStateText);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
	CloseLabel->SetText(NSLOCTEXT("ClcTeleport", "Close", "关闭"));
	CloseButton->SetContent(CloseLabel);
	Layout->AddChildToVerticalBox(CloseButton);
}
