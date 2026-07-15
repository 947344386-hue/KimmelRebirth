// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcMerchantPersonality.generated.h"

/**
 * 商人性格定义——鹰眼可见的固有 tag，驱动该商人的撒谎倾向和演技。
 *
 * 不直接编码摊位好坏，而是告诉玩家「该给这个商人的嘴上和动作打多少折」：
 * 老油条嘴上重骗且动作难读、新手想骗但动作穿帮。心理话始终是诚实锚点。
 *
 * 架构支持任意数量性格（DA 引用，不硬编码枚举）——先做4种：老实人/老油条/新手/神秘人。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantPersonality : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 鹰眼模式下头顶显示的性格 tag 文案（如「老油条」） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality")
	FText TagText;

	/** 撒谎倾向 [0,1]——非好摊按此概率被嘴上说成好摊（嘴上话术偏离真实档位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeceptionLevel = 0.5f;

	/** 演技 [0,1]——单块微反应被藏住的概率（演技高=动作难读、少泄漏诚实信号） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActingSkill = 0.5f;
};
