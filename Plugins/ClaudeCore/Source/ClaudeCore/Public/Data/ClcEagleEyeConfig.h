// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcEagleEyeConfig.generated.h"

/**
 * 鹰眼能力配置
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcEagleEyeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 激活持续时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float ActiveDuration = 5.0f;

	/** 冷却时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float CooldownDuration = 6.0f;

	/** 能量球最小缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float BallMinScale = 1.0f;

	/** 能量球最大缩放 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float BallMaxScale = 4.0f;

	/** 能量球缩放映射的参考价值——价值等于该值附近时球缩放为最大值 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float BallReferenceValue = 10000.0f;

	/** 扫描摊位范围半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float ScanRadius = 3000.0f;
};
