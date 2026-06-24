// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcEagleEyeConfig.generated.h"

UCLASS(BlueprintType)
class CLAUDECORE_API UClcEagleEyeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float ActiveDuration = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float CooldownDuration = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float BallMinScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float BallMaxScale = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float BallReferenceValue = 10000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye") float ScanRadius = 3000.0f;
};