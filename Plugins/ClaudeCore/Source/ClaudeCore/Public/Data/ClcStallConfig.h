// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcStallConfig.generated.h"

UCLASS(BlueprintType)
class CLAUDECORE_API UClcStallConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall") int32 StonesPerStall = 6;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall") FVector2D StoneScaleRange = FVector2D(0.8f, 1.3f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall") float StoneSpreadRadius = 150.0f;
};