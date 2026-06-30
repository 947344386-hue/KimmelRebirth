// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcJadeTypes.h"
#include "ClcStoneConfig.generated.h"

/**
 * 单个产地对各种水档位的权重加成
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcOriginGradeBonus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonus")
	FString Origin;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonus")
	TMap<EClcJadeGrade, float> GradeBonuses;
};

/**
 * 定价与石头生成参数——全部可配置，设计师在编辑器中填表
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcStoneConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- 生成参数 ----

	/** 种水系数：豆1.0 / 糯2.0 / 冰4.0 / 玻8.0（可调） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TMap<EClcJadeGrade, float> GradeValueMultiplier;

	/** 各种水档位的生成权重（豆:40, 糯:30, 冰:20, 玻:10 为例） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TMap<EClcJadeGrade, float> GradeRollWeights;

	/** 产地列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TArray<FString> Origins;

	/** 产地对种水的软关联提升系数（每个条目：产地 + 各档位加成权重） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	TArray<FClcOriginGradeBonus> OriginGradeBonuses;

	/** 绿面积比例的随机范围 [Min, Max] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FVector2D GreenRatioRange = FVector2D(0.05f, 0.7f);

	/** 黑（杂裂）面积比例的随机范围 [Min, Max] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FVector2D BlackRatioRange = FVector2D(0.0f, 0.4f);

	/** 大块连续绿占绿面积比例的随机范围 [Min, Max] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FVector2D LargestPatchRatioRange = FVector2D(0.3f, 0.95f);

	// ---- 定价参数 ----

	/** 单位面积玉肉基础单价 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float PricePerUnitArea = 100.0f;

	/** 未开窗原石的单位面积保底价（远低于 PricePerUnitArea） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float PriceFloorPerArea = 5.0f;

	/** 单位面积杂裂惩罚扣分 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float PenaltyPerUnitBlack = 20.0f;

	/** 大块绿暴击系数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float ContinuityBonusFactor = 2.0f;

	/** 大块连续绿面积阈值（超过该面积才触发暴击） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float ContinuityAreaThreshold = 50.0f;

	/** 赌价激活的最小开窗比例（>= 该值才计算剩余赌价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float GamblingRThreshold = 0.5f;

	/** 剩余赌价系数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float GamblingKCoefficient = 0.4f;

	/** 隐藏溢价系数（购买标价时，理论价值乘以该系数打入标价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float HiddenPremiumFactor = 0.15f;

	/** 石头标价的基础系数（乘以表面积，再叠加隐藏溢价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float BasePricePerArea = 50.0f;

	// ---- GM / 调试 ----

	/** 玩家初始金币 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GM")
	int32 InitialGold = 50000;
};
