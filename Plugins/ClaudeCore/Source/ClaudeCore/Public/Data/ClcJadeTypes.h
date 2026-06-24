// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcJadeTypes.generated.h"

UENUM(BlueprintType)
enum class EClcJadeGrade : uint8
{
	Bean = 0 UMETA(DisplayName = "豆种"),
	Glutinous = 1 UMETA(DisplayName = "糯种"),
	Ice = 2 UMETA(DisplayName = "冰种"),
	Glass = 3 UMETA(DisplayName = "玻种")
};

USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneInternalData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) EClcJadeGrade Grade = EClcJadeGrade::Bean;
	UPROPERTY(BlueprintReadOnly) float SurfaceArea = 0.0f;
	UPROPERTY(BlueprintReadOnly) float GreenRatio = 0.0f;
	UPROPERTY(BlueprintReadOnly) float BlackRatio = 0.0f;
	UPROPERTY(BlueprintReadOnly) float LargestGreenPatchRatio = 0.0f;
	UPROPERTY(BlueprintReadOnly) FString Origin;
	UPROPERTY(BlueprintReadOnly) int32 PurchasePrice = 0;
	float TheoreticalValue = 0.0f;
};

USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneRuntimeData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FClcStoneInternalData Internal;
	UPROPERTY(BlueprintReadOnly) float AccumulatedOpenedArea = 0.0f;
	UPROPERTY(BlueprintReadOnly) float OpenedGreenArea = 0.0f;
	UPROPERTY(BlueprintReadOnly) float OpenedBlackArea = 0.0f;
	UPROPERTY(BlueprintReadOnly) float LargestExposedGreenPatch = 0.0f;
	UPROPERTY(BlueprintReadOnly) FString DisplayName;
	UPROPERTY() int32 BackpackIndex = -1;
};