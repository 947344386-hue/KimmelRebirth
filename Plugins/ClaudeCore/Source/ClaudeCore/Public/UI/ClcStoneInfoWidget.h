// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneInfoWidget.generated.h"

/**
 * 石头信息卡片——摄像机瞄准石头时显示于屏幕一侧
 */
UCLASS()
class CLAUDECORE_API UClcStoneInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcStoneInfo")
	void ShowInfo(const FClcStoneRuntimeData& StoneData);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcStoneInfo")
	void HideInfo();
};
