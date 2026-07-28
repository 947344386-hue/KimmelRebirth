// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcTeleportMenuWidget.generated.h"

class AClcTeleportPoint;
class UButton;
class UClcTeleportEntryWidget;
class UClcTeleportSubsystem;
class UPanelWidget;
class UTextBlock;

UCLASS()
class CLAUDECORE_API UClcTeleportMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcTeleportMenuWidget(const FObjectInitializer& ObjectInitializer);

	void InitializeMenu(UClcTeleportSubsystem* InSubsystem, const FText& InTitle,
		const TArray<AClcTeleportPoint*>& InDestinations);

	void SelectDestination(AClcTeleportPoint* Destination);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UPanelWidget> DestinationContainer;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UTextBlock> EmptyStateText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport|UI")
	TSubclassOf<UClcTeleportEntryWidget> EntryWidgetClass;

private:
	UFUNCTION()
	void HandleCloseClicked();

	void BuildDefaultLayout();
	void RequestClose();

	TWeakObjectPtr<UClcTeleportSubsystem> TeleportSubsystem;
};
