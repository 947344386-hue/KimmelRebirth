// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcJadeTypes.generated.h"

class UStaticMesh;

/**
 * 种水四档：豆、糯、冰、玻璃
 */
UENUM(BlueprintType)
enum class EClcJadeGrade : uint8
{
	Bean = 0 UMETA(DisplayName = "豆种"),
	Glutinous = 1 UMETA(DisplayName = "糯种"),
	Ice = 2 UMETA(DisplayName = "冰种"),
	Glass = 3 UMETA(DisplayName = "玻种")
};

/**
 * 分布图逐像素的材质类别（四值）。取代旧的 0/1/2 三值语义：
 * 全图不再“非绿即黑”，而是玉肉 / 杂质 / 裂纹 / 废肉四种独立场。
 *
 * 注意：擦石材质 M_StoneOpening 仍按双通道 TypeTex 混合（R=玉 / G=杂），
 * 因此杂质/裂纹/废肉在视觉上都走 Junk PBR，靠“小簇 vs 细线 vs 大片”的
 * 几何形态自然区分；价值与惩罚在 C++ 侧按四类分别统计。
 */
enum EClcDistVoxel : uint8
{
	HostWaste = 0,  //!< 废肉/底岩：中性，既非玉也非缺陷
	JadeBody  = 1,  //!< 玉肉：大尺度连续，价值来源（沿用原绿色计数口径）
	Impurity  = 2,  //!< 杂质：聚簇，轻度惩罚（沿用原杂裂计数口径）
	Crack     = 3,  //!< 裂纹：细线网络，重度惩罚
};

/**
 * 石头操作阶段（互斥门控）：一旦被一个工作台操作过，另一个台封死。
 * 与 bHaggleResolved 正交——锁价可叠加在 Windowed 或 Cut 之上（终端态，不可再操作）。
 *   Unworked → 可擦石也可解石；
 *   Windowed → 只能继续擦石，不能解石；
 *   Cut      → 只能继续解石，不能擦石。
 */
UENUM(BlueprintType)
enum class EClcStonePhase : uint8
{
	Unworked UMETA(DisplayName = "未操作"),
	Windowed UMETA(DisplayName = "已擦石"),
	Cut      UMETA(DisplayName = "已解石"),
};

/**
 * 石头表面材质分布图——每像素标记玉肉/杂质/裂纹/废肉（EClcDistVoxel）
 * 分辨率 256×256，确定性算法从 Seed 生成
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneDistributionMap
{
	GENERATED_BODY()

	static constexpr int32 Resolution = 256;

	/** 分布数据：Resolution*Resolution 字节，取值为 EClcDistVoxel（0=废肉 1=玉肉 2=杂质 3=裂纹） */
	UPROPERTY()
	TArray<uint8> Data;

	FClcStoneDistributionMap() { Data.Init(0, Resolution * Resolution); }

	/** 按像素坐标获取材质类型 */
	uint8 GetPixel(int32 X, int32 Y) const
	{
		if (X < 0 || X >= Resolution || Y < 0 || Y >= Resolution) return 0;
		return Data[Y * Resolution + X];
	}

	/**
	 * 确定性生成分布图（有机缺陷体模型）：
	 * 默认整块是玉肉，再生成 DefectCount 个**连续不规则缺陷体**（像闪电/海星）把玉占掉。
	 * TargetCoverage 是缺陷目标覆盖率（实际值由 Measure 实测，定价只认实测）。
	 * 每个缺陷体的形态由其分到的体积决定：体积小→蛛网/闪电（细丝多方向分叉），
	 * 体积大→团球伸出触手（粗核心+放射触手）。
	 * OutActuals 写回玉/缺陷像素数与最大玉肉连通域像素数。
	 */
	struct FMeasureResult
	{
		int32 JadePixels = 0;
		int32 CrackPixels = 0;
		int32 LargestJadePatchPixels = 0;
	};
	static FClcStoneDistributionMap Generate(int32 Seed, int32 DefectCount,
		float TargetCoverage, FMeasureResult& OutActuals);

	/** 实测当前分布：各类像素数 + 最大玉肉连通域像素数（定价与自检的权威输入） */
	FMeasureResult Measure() const;
};

/**
 * 石头生成时的内在数据——一旦生成就不可变
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneInternalData
{
	GENERATED_BODY()

	/** 决定绿/黑分布的程序化种子 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 Seed = 0;

	/** 皮壳类型索引——指向 DA_ShellTextureConfig 的条目，决定外观纹理 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 ShellTypeIndex = 0;

	/** 种水档位 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	EClcJadeGrade Grade = EClcJadeGrade::Bean;

	/** 商人嘴上吹的黑话句——生成时按真实种水+商人欺骗倾向定档后从黑话池取，名字与气泡同读它（不一定如实，半可信） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FString ClaimedPitch;

	/** 石头总表面积（平方单位，从Mesh Bounds推算） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float SurfaceArea = 0.0f;

	/** 估算重量（公斤，按 Mesh 包围盒椭球体积 × 翡翠密度 3.3 g/cm³ 换算，四舍五入到整公斤，仅供展示） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 WeightKg = 0;

	/** 玉肉（绿色）面积占全石表面积的比例 [0,1]——生成后由 Measure 写入实际值，定价权威输入 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float GreenRatio = 0.0f;

	/** 杂质面积占全石表面积的比例 [0,1]——聚簇缺陷，轻度惩罚（生成后实测） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float ImpurityRatio = 0.0f;

	/** 裂纹面积占全石表面积的比例 [0,1]——细线网络，重度惩罚（生成后实测） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float CrackRatio = 0.0f;

	/**
	 * 杂裂（杂质+裂纹）合计占比 [0,1]，GreenRatio+BlackRatio<=1。
	 * 保留字段供旧读取；实际语义=ImpurityRatio+CrackRatio，不再独立驱动定价。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float BlackRatio = 0.0f;

	/** 最大单块连续玉肉占全石玉肉面积的比例 [0,1]——生成后实测，连续性判定权威输入 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float LargestGreenPatchRatio = 0.0f;

	/** 产地名称 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FString Origin;

	/** 玩家购买时支付的价格（即标价，含隐藏溢价） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 PurchasePrice = 0;

	/** 理论全开价值（用于鹰眼和定价计算，内部使用） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float TheoreticalValue = 0.0f;

	/** Phase 2：UV 空间材质分布图（皮壳/绿玉/杂裂），确定性生成 */
	UPROPERTY()
	FClcStoneDistributionMap DistributionMap;

	/** 石头模型——生成时确定，购买/擦石流程沿用同一 Mesh */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	TSoftObjectPtr<UStaticMesh> StoneMesh;

	/** 石头缩放——生成时确定 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float MeshScale = 1.0f;
};

/**
 * 切平面记录（mesh 局部空间）——解石台退出时序列化保存，再上台时从 Seed 重生成
 * 3D 体素场后回放这些切平面重建剩余体几何。铡刀切平面在石头局部空间里轴对齐
 * （法线 ∈ {X,Y,Z}，取决于上台时哪条 bbox 轴对齐到移动方向），只有 Distance 变化。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcCutPlaneRecord
{
	GENERATED_BODY()

	/** 切平面法线（mesh 局部空间，归一化，轴对齐） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcCut")
	FVector Normal = FVector(1, 0, 0);

	/** 切平面距离（mesh 局部空间，FPlane 参数 d：Normal·P + Distance = 0；<0 一侧为切走侧） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcCut")
	float Distance = 0.0f;

	/** 切走的是 Normal·P+Distance<0 一侧（true）还是正侧（false）——重建 PMC 时据此保留另一侧 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcCut")
	bool bRemovedNegative = true;
};

/**
 * 石头运行时状态——随擦石推进而变化
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneRuntimeData
{
	GENERATED_BODY()

	/** 不变的内在数据 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FClcStoneInternalData Internal;

	/** 已累计擦石面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float AccumulatedOpenedArea = 0.0f;

	/** 已擦石中暴露的玉肉面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedGreenArea = 0.0f;

	/** 已擦石中暴露的杂质面积（聚簇缺陷） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedImpurityArea = 0.0f;

	/** 已擦石中暴露的裂纹面积（细线缺陷，重度惩罚） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedCrackArea = 0.0f;

	/** 已擦石中暴露的杂裂（杂质+裂纹）合计面积——保留旧字段，=OpenedImpurityArea+OpenedCrackArea */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedBlackArea = 0.0f;

	/** 当前最大已暴露绿色连通域面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float LargestExposedGreenPatch = 0.0f;

	/** 展示名（生成时随机分配，如"老坑沙皮 #42"） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FString DisplayName;

	/** 讨价还价锁定的售价（<0=未锁定；≥0=已锁价，按此售出，不再随擦石重算） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 HaggleLockedPrice = -1;

	/** 是否已讨价还价结算（锁定后不可再擦石/再讨价，玩家需手动确认售出） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	bool bHaggleResolved = false;

	/**
	 * 操作阶段（互斥门控）——决定该石头能上哪个台。
	 * 与 bHaggleResolved 正交：锁价可叠加在 Windowed/Cut 之上（终端态）。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	EClcStonePhase Phase = EClcStonePhase::Unworked;

	/** 切平面记录列表（局部空间）——解石台退出时保存，再上台从 Seed 重生成体素场后回放重建 */
	UPROPERTY()
	TArray<FClcCutPlaneRecord> CutPlanes;

	/** 累计切走总体积（cm³）——每次 ExecuteCut 后累加，供切石预算分摊与回收价使用 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float ExposedCutVolume = 0.0f;

	/** 累计切走玉肉体积（cm³）——每次 ExecuteCut 后累加，切石预算按玉肉份额分配 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float ExposedJadeVolume = 0.0f;

	/** 累计切走裂纹体积（cm³）——每次 ExecuteCut 后累加，仅用于展示/审计，不再参与货币公式 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float ExposedCrackVolume = 0.0f;

	/** 剩余未切总体积（cm³）——由体素场 CountRemainingVoxels 重算，不靠累加 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float RemainingVolume = 0.0f;

	/** 剩余主体中的玉肉体积（cm³）——供理论预算分摊和回收价使用，每次切割后重算 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float RemainingJadeVolume = 0.0f;

	/** 已分配掉的毛切块预算（整数金币）——含尺寸/硬地板扣损，用于增量预算分摊。
	 *  下一刀从 max(Stored, TargetBefore) 起算，兼容旧数据（0 表示最多补到当前累计预算）。 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 ConsumedCutBudget = 0;

	/** 解石台已结算的切块累计金币（玩家实际到手，供 HUD/统计） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	int32 TotalSettledValue = 0;


	/** 遮罩 RT 像素缓冲区（256×256 字节），退出工作台时保存，再进入时恢复 */
	UPROPERTY()
	TArray<uint8> SavedMaskBuffer;
};

/**
 * 背包悬浮 tips 聚合数据——C++ 侧 BuildTooltipInfo 一次性算好，BP 只管渲染。
 * 皮壳/种水互斥：bOpenedToJade=true 时填 GradeText，false 时填 ShellName。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneTooltipInfo
{
	GENERATED_BODY()

	/** 展示名，如"老坑沙皮 #42" */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	FString DisplayName;

	/** 产地，如"木那" */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	FString Origin;

	/** 是否已开到玉（OpenedGreenArea > 0）——决定显示皮壳还是种水 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	bool bOpenedToJade = false;

	/** 未开到玉时填皮壳名（如"黄沙皮"），已开到玉时留空 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	FString ShellName;

	/** 已开到玉时填种水档位（如"冰种"），未开到玉时留空 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	FString GradeText;

	/** 当前回收价（CalculateSalePrice 实时算） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	int32 CurrentValue = 0;

	/** 购入价 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcTooltip")
	int32 PurchasePrice = 0;
};
