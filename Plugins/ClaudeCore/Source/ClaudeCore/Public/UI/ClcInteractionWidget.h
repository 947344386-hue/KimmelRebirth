// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcInteractionWidget.generated.h"
UCLASS()
class CLAUDECORE_API UClcInteractionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void SetStateHidden();
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void SetStateInRange();
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void SetStateSelected();
};