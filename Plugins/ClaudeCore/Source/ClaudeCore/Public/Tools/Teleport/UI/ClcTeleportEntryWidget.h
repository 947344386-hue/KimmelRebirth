// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcTeleportEntryWidget.generated.h"

class AClcTeleportPoint;
class UButton;
class UClcTeleportMenuWidget;
class UTextBlock;

UCLASS()
class CLAUDECORE_API UClcTeleportEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeEntry(AClcTeleportPoint* InDestination, UClcTeleportMenuWidget* InOwnerMenu);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Teleport|UI")
	TObjectPtr<UTextBlock> NameText;

private:
	UFUNCTION()
	void HandleClicked();

	void BuildDefaultLayout();

	TWeakObjectPtr<AClcTeleportPoint> Destination;
	TWeakObjectPtr<UClcTeleportMenuWidget> OwnerMenu;
};
