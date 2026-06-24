// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcBackpackWidget.generated.h"
UCLASS()
class CLAUDECORE_API UClcBackpackWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent) void RefreshDisplay(const TArray<FClcStoneRuntimeData>& Stones, int32 Gold);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoneSelected, int32, StoneIndex);
	UPROPERTY(BlueprintAssignable) FOnStoneSelected OnStoneSelected;
};