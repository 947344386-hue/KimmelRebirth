// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/Teleport/UI/ClcTeleportEntryWidget.h"
#include "Tools/Teleport/UI/ClcTeleportMenuWidget.h"
#include "Tools/Teleport/ClcTeleportPoint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UClcTeleportEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcTeleportEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UClcTeleportEntryWidget::HandleClicked);
		SelectButton->OnClicked.AddDynamic(this, &UClcTeleportEntryWidget::HandleClicked);
	}
}

void UClcTeleportEntryWidget::NativeDestruct()
{
	if (SelectButton)
	{
		SelectButton->OnClicked.RemoveDynamic(this, &UClcTeleportEntryWidget::HandleClicked);
	}
	Destination.Reset();
	OwnerMenu.Reset();
	Super::NativeDestruct();
}

void UClcTeleportEntryWidget::InitializeEntry(AClcTeleportPoint* InDestination,
	UClcTeleportMenuWidget* InOwnerMenu)
{
	Destination = InDestination;
	OwnerMenu = InOwnerMenu;
	if (NameText && InDestination)
	{
		NameText->SetText(InDestination->GetDisplayName());
	}
}

void UClcTeleportEntryWidget::HandleClicked()
{
	if (UClcTeleportMenuWidget* Menu = OwnerMenu.Get())
	{
		Menu->SelectDestination(Destination.Get());
	}
}

void UClcTeleportEntryWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
	WidgetTree->RootWidget = SelectButton;

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetJustification(ETextJustify::Center);
	SelectButton->SetContent(NameText);
}
