// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/ClcMerchantTypes.h"
#include "ClcMerchantTalkConfig.generated.h"

class UClcMerchantPersonality;

/**
 * 嘴上话术池配置——文字 = f(性格, 交互状态, 声称档位)。
 *
 * PickLine 带降级 fallback：同状态同档位下优先性格精确匹配，其次 Personality=空的通用池。
 * 档位和状态始终精确（不会跨档位 fallback 选错话术）。
 * 体感不对在编辑器改话术，不用碰代码。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantTalkConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UClcMerchantTalkConfig();

	/** 按 (性格, 状态, 声称档位) 取一句话术；未配置走通用池 fallback，全空返回空 FText */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantTalk")
	FText PickLine(UClcMerchantPersonality* Personality, ETalkState State, EClcStallTier ClaimedTier) const;

protected:
	/** 话术池（编辑器按 性格+状态+声称档位 组合填；Personality 留空=通用池） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Talk", meta = (TitleProperty = "{State}|{ClaimedTier}"))
	TArray<FClcTalkPool> TalkPools;
};
