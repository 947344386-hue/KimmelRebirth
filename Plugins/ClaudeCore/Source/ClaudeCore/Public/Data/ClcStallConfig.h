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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "50.0", ClampMax = "500.0")) float UnitCellSize = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "0.0", ClampMax = "3.0")) float MarginCells = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall") FVector2D StoneScaleRange = FVector2D(0.6f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "0.0", ClampMax = "0.5")) float CellJitterRatio = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall") float StallThicknessScale = 0.2f;
};