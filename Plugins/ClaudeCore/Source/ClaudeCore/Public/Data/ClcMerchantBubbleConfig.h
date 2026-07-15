// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/ClcMerchantTypes.h"
#include "ClcMerchantBubbleConfig.generated.h"

/**
 * 商人气泡文字池配置——文字 = f(当前摊位档位, 上次购买结果)。
 * 3 档 × 3 结果 = 9 个状态，每状态挂一组文字，运行时随机选一句。
 * 体感不对在编辑器改文案即可，不用碰代码。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantBubbleConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 按 (档位, 购买结果) 取一句文字；未配置返回空 FText */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantBubble")
	FText PickLine(EClcStallTier Tier, EClcPurchaseOutcome Outcome) const;

protected:
	/** 9 个状态的文字池（编辑器里按 Tier + LastOutcome 组合填） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bubble", meta = (TitleProperty = "{Tier}|{LastOutcome}"))
	TArray<FClcBubbleStatePool> BubblePools;
};
