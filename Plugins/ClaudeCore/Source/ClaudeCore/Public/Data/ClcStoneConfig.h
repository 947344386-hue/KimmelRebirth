// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcJadeTypes.h"
#include "ClcStoneConfig.generated.h"

USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcOriginGradeBonus
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Origin;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<EClcJadeGrade, float> GradeBonuses;
};

UCLASS(BlueprintType)
class CLAUDECORE_API UClcStoneConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") TMap<EClcJadeGrade, float> GradeValueMultiplier;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") TMap<EClcJadeGrade, float> GradeRollWeights;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") TArray<FString> Origins;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") TArray<FClcOriginGradeBonus> OriginGradeBonuses;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") FVector2D GreenRatioRange = FVector2D(0.05f, 0.7f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") FVector2D BlackRatioRange = FVector2D(0.0f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation") FVector2D LargestPatchRatioRange = FVector2D(0.3f, 0.95f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float PricePerUnitArea = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float PriceFloorPerArea = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float PenaltyPerUnitBlack = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float ContinuityBonusFactor = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float ContinuityAreaThreshold = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float GamblingRThreshold = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float GamblingKCoefficient = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float HiddenPremiumFactor = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing") float BasePricePerArea = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GM") int32 InitialGold = 50000;
};