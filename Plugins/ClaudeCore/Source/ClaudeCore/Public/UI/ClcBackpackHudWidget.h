// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcBackpackHudWidget.generated.h"

class UTextBlock;

/**
 * 背包常驻金币条——屏幕顶部的金币/石头数量显示。
 *
 * 默认布局可用（无需 WBP）：金币 + 数量 + 石头 + N/200，居中置顶。
 * 可由 WBP_BackpackHud（父类 UClcBackpackHudWidget，绑定 GoldText/StoneCountText）换皮，
 * 让外观与 WBP_Backpack 的 title 栏一致，实现"金币条 ↔ 背包 title 栏"的互斥体感。
 *
 * 生命周期由 UClcBackpackSubsystem 管理：Initialize 创建并 AddToViewport(50)；
 * 背包打开时隐藏（与背包 title 栏互斥），关闭时恢复，金币/石头变化时实时刷新。
 */
UCLASS()
class CLAUDECORE_API UClcBackpackHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcBackpackHudWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;

	/** 更新金币数字 */
	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void SetGold(int32 InGold);

	/** 更新石头数量（当前/上限） */
	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void SetStoneCount(int32 Current, int32 Max);

protected:
	/** 金币数字文本——WBP 可选绑定 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcBackpack")
	TObjectPtr<UTextBlock> GoldText;

	/** 石头数量文本——WBP 可选绑定 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcBackpack")
	TObjectPtr<UTextBlock> StoneCountText;

private:
	void BuildDefaultLayout();
};
