// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "ClcKeyPromptWidget.generated.h"

class UClcKeyPromptEntryWidget;
class UPanelWidget;

UCLASS()
class CLAUDECORE_API UClcKeyPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcKeyPromptWidget(const FObjectInitializer& ObjectInitializer);

	/** 由 UClcKeyPromptSubsystem 调用，按 SortPriority 排序后重建条目。 */
	void Refresh(const TArray<FClcKeyPrompt>& Prompts);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcKeyPrompt")
	TObjectPtr<UPanelWidget> PromptContainer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClcKeyPrompt")
	TSubclassOf<UClcKeyPromptEntryWidget> EntryWidgetClass;

private:
	void BuildDefaultLayout();
};
