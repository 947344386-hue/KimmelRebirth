// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "ClcKeyPromptEntryWidget.generated.h"

class UTextBlock;

UCLASS()
class CLAUDECORE_API UClcKeyPromptEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcKeyPromptEntryWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "ClcKeyPrompt")
	void SetPrompt(const FKey& Key, const FText& Label);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcKeyPrompt")
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcKeyPrompt")
	TObjectPtr<UTextBlock> LabelText;

private:
	void BuildDefaultLayout();
};
