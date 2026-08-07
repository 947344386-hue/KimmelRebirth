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
 * 商人吹卖黑话池——一个声称种水档位对应一组候选黑话
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcPitchPool
{
	GENERATED_BODY()

	/** 声称种水档位 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pitch")
	EClcJadeGrade Grade = EClcJadeGrade::Bean;

	/** 该档位候选黑话（生成时随机取 2 条拼成 ClaimedPitch） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pitch")
	TArray<FText> Phrases;
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

	/** 缺陷覆盖率目标范围 [Min,Max]——占比是好坏的主区分。最好~Min(5%)，最差~Max(85%) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	FVector2D CrackCoverageRange = FVector2D(0.05f, 0.85f);

	// ---- 定价参数 ----
	// 系数按 SA≈16000（半径 40cm 球 ×0.8）标定。若改 Mesh 尺寸需同步重标定。
	// 连续暴击阈值/系数已内化为算法常量（见 ClcStoneMarketSubsystem.cpp），不再暴露。

	/** 单位面积玉肉基础单价 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float PricePerUnitArea = 2.0f;

	/** 未擦石原石的保底折扣系数（保底价 = TheoreticalValue × 此系数，杂裂多的石头自然低） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float UnopenedFloorDiscountFactor = 0.1f;

	/** 单位面积缺陷惩罚扣分（有机缺陷体模型下唯一惩罚项，缺陷越密价值越低） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float PenaltyPerUnitCrack = 5.0f;

	/** 净外推赌价系数——把已开区域净价值密度外推到未开区域的强度。
	 *  越大越刺激（富窗吹高、穷窗砸低都更猛）；全开时赌价=0，回收价恒=理论价值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float GamblingKCoefficient = 0.8f;

	/** 隐藏溢价系数（购买标价时，理论价值乘以该系数打入标价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float HiddenPremiumFactor = 0.15f;

	/** 石头标价的基础系数（乘以表面积，再叠加隐藏溢价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing")
	float BasePricePerArea = 0.1f;

	// ---- 解石（3D 体积）定价参数 ----

	/** 单位体积玉肉基础单价（cm³）——初值 0.0005，待实测标定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting")
	float PricePerUnitVolume = 0.0005f;

	/** 单位体积缺陷惩罚扣分——解石版，应和 PricePerUnitVolume 同级量纲（默认玉肉的 0.5 倍） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting")
	float PenaltyPerUnitCrackVolume = 0.00025f;

	/** 切块最小有价值体积（cm³）——低于此体积的切块直接归零，防薄片速切 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting")
	float MinVolumeForValue = 500.0f;

	// ---- 命名/话术 ----

	/**
	 * 商人吹卖黑话池——按声称种水档位(EClcJadeGrade)索引。
	 * 生成时按真实种水+商人欺骗倾向 roll 出声称档，从对应池随机取 2 条拼成 ClaimedPitch。
	 * 池空时回退到档位 DisplayName。文案在此 DA 调整，勿硬编码。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Naming", meta = (TitleProperty = "{Grade}"))
	TArray<FClcPitchPool> JadePitchPool;

	// ---- GM / 调试 ----

	/** 玩家初始金币 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GM")
	int32 InitialGold = 50000;
};
