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
 * 石头表面材质分布图——每像素标记皮壳/绿玉/杂裂
 * 分辨率 256×256，确定性算法从 Seed 生成
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneDistributionMap
{
	GENERATED_BODY()

	static constexpr int32 Resolution = 256;

	/** 分布数据：Resolution*Resolution 字节，0=皮壳 1=绿玉 2=杂裂 */
	UPROPERTY()
	TArray<uint8> Data;

	FClcStoneDistributionMap() { Data.Init(0, Resolution * Resolution); }

	/** 按像素坐标获取材质类型 */
	uint8 GetPixel(int32 X, int32 Y) const
	{
		if (X < 0 || X >= Resolution || Y < 0 || Y >= Resolution) return 0;
		return Data[Y * Resolution + X];
	}

	/** 按归一化 UV（0-1）采样材质类型 */
	uint8 SampleUV(float U, float V) const
	{
		const int32 X = FMath::Clamp(FMath::RoundToInt(U * (Resolution - 1)), 0, Resolution - 1);
		const int32 Y = FMath::Clamp(FMath::RoundToInt(V * (Resolution - 1)), 0, Resolution - 1);
		return Data[Y * Resolution + X];
	}

	bool IsGreen(int32 X, int32 Y) const { return GetPixel(X, Y) == 1; }
	bool IsBlack(int32 X, int32 Y) const { return GetPixel(X, Y) == 2; }
	bool IsShell(int32 X, int32 Y) const { return GetPixel(X, Y) == 0; }

	/** 确定性生成分布图。OutActualLargestPatchRatio 返回实际测得的最大连续绿块占比 */
	static FClcStoneDistributionMap Generate(int32 Seed, float GreenRatio, float BlackRatio, float& OutActualLargestPatchRatio);
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

	/** 玉石绿色面积占全石表面积的比例 [0, 1] */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float GreenRatio = 0.0f;

	/** 杂裂黑色面积占全石表面积的比例 [0, 1]，GreenRatio + BlackRatio <= 1.0 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float BlackRatio = 0.0f;

	/** 最大单块连续绿块占全石绿色面积的比例 [0, 1]，用于连续性判定 */
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

	/** 石头模型——生成时确定，购买/开窗流程沿用同一 Mesh */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	TSoftObjectPtr<UStaticMesh> StoneMesh;

	/** 石头缩放——生成时确定 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float MeshScale = 1.0f;
};

/**
 * 石头运行时状态——随开窗推进而变化
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneRuntimeData
{
	GENERATED_BODY()

	/** 不变的内在数据 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FClcStoneInternalData Internal;

	/** 已累计开窗面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float AccumulatedOpenedArea = 0.0f;

	/** 已开窗中暴露的绿色面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedGreenArea = 0.0f;

	/** 已开窗中暴露的黑色面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float OpenedBlackArea = 0.0f;

	/** 当前最大已暴露绿色连通域面积 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	float LargestExposedGreenPatch = 0.0f;

	/** 展示名（生成时随机分配，如"老坑沙皮 #42"） */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStone")
	FString DisplayName;


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
