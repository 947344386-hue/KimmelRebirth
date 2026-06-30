// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcBackpackWidget.generated.h"

/**
 * 背包列表UI——按键呼出/关闭
 */
UCLASS()
class CLAUDECORE_API UClcBackpackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 刷新背包显示 */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcBackpack")
	void RefreshDisplay(const TArray<FClcStoneRuntimeData>& Stones, int32 Gold);

	/** Widget中点击石头回调——通知背包子系统选中了哪块石头 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoneSelected, int32, StoneIndex);
	UPROPERTY(BlueprintAssignable, Category = "ClcBackpack")
	FOnStoneSelected OnStoneSelected;

	/** StoneEntry蓝图调用此函数来触发选石——内部广播 OnStoneSelected 委托 */
	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void SelectStone(int32 StoneIndex);
};
