// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneInfoWidget.generated.h"
UCLASS()
class CLAUDECORE_API UClcStoneInfoWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void ShowInfo(const FClcStoneRuntimeData& StoneData);
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void HideInfo();
};