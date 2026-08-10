// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcStoneVoxelField3D.generated.h"

class UStaticMesh;

/**
 * 3D 体素场——铡刀解石台的内部品质模型。与 2D FClcStoneDistributionMap 独立：
 * 因阶段互斥（EClcStonePhase），同一块石头只会走打磨(2D)或解石(3D)之一，两者不在
 * 同一实例上共存，故互不重写。
 *
 * 从 InternalData.Seed 懒生成（仅在石头上台解石时生成，不存入背包数据）。默认整块是
 * 玉肉，再生成若干连续不规则缺陷体（3D 版有机缺陷体：小→蛛网/闪电、大→团球触手）。
 * 按所在 StaticMesh 的实际形状体素化（OccupancyMask 标记 mesh 内/外），缺陷体只在内部
 * 体素生成；切平面在 mesh 局部空间同时切几何(PMC)与体素(本场的 RemoveMask)。
 *
 * 体素值取 EClcDistVoxel（0=废肉 HostWaste / 1=玉肉 JadeBody / 2=杂质 Impurity / 3=裂纹 Crack）；
 * 当前生成器只产 JadeBody + Crack（与 2D 裂纹切割模型同构）。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneVoxelField3D
{
	GENERATED_BODY()

	/** 默认分辨率（48³≈110KB/石，细节与性能平衡；可由调用方覆盖） */
	static constexpr int32 DefaultResolution = 48;

	/** 分辨率 N → N³ 个体素 */
	int32 Resolution = DefaultResolution;

	/** 体素数据：Resolution³ 字节，取值为 EClcDistVoxel */
	UPROPERTY()
	TArray<uint8> Data;

	/** 占用掩码：标记哪些体素在 mesh 内部（外部=废肉不参与缺陷生成与计数） */
	TArray<uint8> OccupancyMask;

	/** 已切走掩码（与 Data 等长，1=该体素已被切走、不参与剩余统计） */
	TArray<uint8> RemoveMask;

	/** 场在局部空间的起点（mesh local bounds min） */
	FVector GridOrigin = FVector::ZeroVector;

	/** 场在局部空间的范围（mesh local bounds size） */
	FVector GridExtent = FVector(100.0f, 100.0f, 100.0f);

	/** 单体素在局部空间的尺寸（GridExtent/Resolution） */
	FVector VoxelSize = FVector::OneVector;

	/** 单体素体积（cm³，VoxelSize 三分量之积） */
	float VoxelVolume = 1.0f;

	// ---- 生成 ----

	/**
	 * 确定性 3D 生成：按 mesh 实际形状体素化 + 缺陷体生成。
	 * Mesh 提供 bounds（GridOrigin/GridExtent）与 LOD0 三角形（体素化）；
	 * Seed 驱动确定性；DefectCount/TargetCoverage 与 2D 版同义。
	 * Mesh 为空或无渲染数据时退化为包络椭球占用。
	 */
	static FClcStoneVoxelField3D Generate(int32 Seed, UStaticMesh* Mesh,
		int32 Resolution, int32 DefectCount, float TargetCoverage);

	// ---- 坐标变换 ----

	/** 体素坐标 → 局部空间位置（体素中心） */
	FVector VoxelToLocal(int32 X, int32 Y, int32 Z) const;

	/** 局部空间位置 → 体素整数坐标（越界分量置为 -1 表示越界） */
	void LocalToVoxelInt(const FVector& LocalPos, int32& OutX, int32& OutY, int32& OutZ) const;

	/** 内部线性索引（X,Y,Z → Data 索引）；越界返回 -1 */
	int32 IndexOf(int32 X, int32 Y, int32 Z) const;

	/** 按局部空间位置采样体素值（三线性插值返回 0~3 浮点，供材质/调试用；越界返回 0） */
	float SampleAtLocalPos(const FVector& LocalPos) const;

	// ---- 解石 ----

	/** 切平面两侧的体素计数（不修改场，供调用方决定哪侧切走） */
	struct FSliceCounts
	{
		int32 NegTotal = 0, NegJade = 0, NegCrack = 0, NegImpurity = 0;
		int32 PosTotal = 0, PosJade = 0, PosCrack = 0, PosImpurity = 0;
	};

	/**
	 * 计算切平面两侧的体素计数（仅统计占用且未切走的体素）。
	 * 平面方程：Normal·P + Distance = 0；<0 一侧为 Neg，>0 一侧为 Pos。
	 */
	FSliceCounts ComputeSliceCounts(const FVector& PlaneNormal, float PlaneDistance) const;

	/**
	 * 标记切走侧的体素为已移除（更新 RemoveMask）。
	 * bRemoveNegativeSide=true 移除 Normal·P+Distance<0 一侧，否则移除正侧。
	 */
	void ApplyCut(const FVector& PlaneNormal, float PlaneDistance, bool bRemoveNegativeSide);

	// ---- 统计 ----

	/** 当前剩余体（占用且未切走）的体素计数 */
	void CountRemainingVoxels(int32& OutTotal, int32& OutJade, int32& OutCrack, int32& OutImpurity) const;

	/** 剩余体中最大连续玉肉连通域体素数（6 邻域 BFS） */
	int32 MeasureLargestJadePatch3D() const;

	/** 剩余体总体积（cm³） */
	float GetRemainingVolume() const;

	/** 调试描述：分辨率/占用率/玉·裂体素/最大连通玉/剩余体积 */
	FString DebugDescribe() const;
};
