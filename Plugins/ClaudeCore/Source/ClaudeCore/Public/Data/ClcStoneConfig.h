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

	/** @deprecated 已废弃——乘法衰减模型不再使用减法惩罚。保留字段兼容旧 DA 序列化，不再参与逻辑运算。 */
	UPROPERTY()
	float PenaltyPerUnitCrack_DEPRECATED = 1.2f;

	// ---- 乘法衰减模型（替代旧减法：T = V_weighted × (1 - DecayRatio)） ----

	/** 裂纹衰减权重 α——CrackRatio × α 进入衰减比。
	 *  有机缺陷体模型下 α=0.8 使中位石头(45%裂)衰减 ~36%，极品(5%裂)衰减 ~4%。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CrackDecayWeight = 0.8f;

	/** 杂质衰减权重 β——ImpurityRatio × β 进入衰减比（杂质伤害弱于裂纹，默认 0.5）。
	 *  衰减比 = Clamp(α×CrackRatio + β×ImpurityRatio, 0.0, MaxDecayRatio) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ImpurityDecayWeight = 0.5f;

	/** 最大衰减比——数学保证 T ≥ V_weighted × (1 - MaxDecayRatio)。
	 *  默认 0.95 → 最差石头仍有 5% 残值，彻底杜绝 T=0。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxDecayRatio = 0.95f;

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

	// ---- 解石定价参数（以 Internal.TheoreticalValue 为唯一经济锚点） ----

	/** 切块最小有价值比例（占原石总体素比）——低于此比例的切块实际金币归零，防薄片速切。
	 *  注意：毛预算仍会被消耗，不会由后续切块补回。默认 0.05 = 5% 原石体积。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinCutRatioForValue = 0.05f;

	/** 切石相对擦石的收益放大系数——Internal.TheoreticalValue × 此系数 = 完整切块总预算。
	 *  标准切法全部切完时，切块累计金币精确收敛到此预算；剩余主体回收只领取理论价份额（不乘此系数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting", meta = (ClampMin = "0.0"))
	float CutValueMultiplier = 1.3f;

	/** 标准切块比例区间 [Min,Max]——切下块体积 / 原石总体积落在此区间内不压缩单价。
	 *  小于 Min 视为薄片速切（过小），大于 Max 视为粗暴大切（过大），均按系数压缩玉肉单价。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D IdealCutRatioRange = FVector2D(0.15f, 0.45f);

	/** 过小切块的玉肉单价压缩下限——r→0 时 SizeFactor 衰减到此值（0.3=压缩到 30% 单价）。
	 *  薄片速切仍有残值但重压缩，避免投机取巧。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UndersizedSizeFactor = 0.3f;

	/** 过大切块的玉肉单价压缩下限——切块比例达到可切较小侧理论上限 0.5 时 SizeFactor 衰减到此值。
	 *  一刀切太粗浪费玉肉，给残值但不鼓励。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OversizedSizeFactor = 0.5f;

	// ---- 切块纯度与缺陷惩罚参数 ----

	/** 板料/粗料折现系数——毛预算先乘此系数再叠加各因子，解决 T 锚定导致单刀通胀过高。默认 0.4。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting|Purity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RoughStoneDiscount = 0.4f;

	/** 纯度指数映射幂次——JadePurity 做 pow(..., PurityExponent) 非线性映射。>1 压低中低纯度切块，<1 反之。默认 1.5。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting|Purity", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float PurityExponent = 1.5f;

	/** 裂纹惩罚权重——加权缺陷率中裂纹的倍率。默认 1.5（裂纹比杂质更伤价）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting|Purity", meta = (ClampMin = "0.0"))
	float CrackPenaltyWeight = 1.5f;

	/** 杂质惩罚权重——加权缺陷率中杂质的倍率。默认 1.0。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pricing|Cutting|Purity", meta = (ClampMin = "0.0"))
	float ImpurityPenaltyWeight = 1.0f;

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
