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

	/** 扫描摊位范围半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye")
	float ScanRadius = 3000.0f;
};
