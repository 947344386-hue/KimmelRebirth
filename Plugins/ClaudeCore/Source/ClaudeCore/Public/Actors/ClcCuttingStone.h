// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "Data/ClcStoneVoxelField3D.h"
#include "ClcCuttingStone.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 解石台上的石头载体（仿 AClcOpeningStone）。用 UProceduralMeshComponent 承载可切外壳：
 *   section 0 = 外壳三角形（M_StoneShell，保留原 UV）；
 *   section 1 = 切面 cap（P2 用顶点色按体素场采样着色验证对齐；P6 换 M_StoneCutFace 采 VolumeTexture）。
 *
 * 内部品质由 FClcStoneVoxelField3D 承载（从 InternalData.Seed 懒生成，与 2D 分布图独立——
 * 因阶段互斥两者不在同一块石头共存）。铡刀切平面在石头局部空间轴对齐（法线 ∈ {X,Y,Z}），
 * 只有 Distance 变化——几何数学是三角形裁剪 + 2D cap 三角化，非任意 CSG。
 *
 * 每次 ExecuteCut：体素场算两侧计数 → 调用方选切走侧 → ApplyCut 更新 RemoveMask →
 * 重建 PMC（外壳裁剪到保留侧 + 切面 cap）。
 */
UCLASS()
class CLAUDECORE_API AClcCuttingStone : public AActor
{
	GENERATED_BODY()

public:
	AClcCuttingStone();

	/**
	 * 初始化：加载源 mesh + 生成 3D 体素场 + 建 PMC（初始为整块外壳）+ 设外壳材质。
	 * 若 StoneData.CutPlanes 非空，从 Seed 重生成场后回放切平面重建剩余体（恢复存档）。
	 * DefectCount/TargetCoverage/VoxelResolution 由调用方（解石台/测试 Actor）传入；
	 * P4 会从 UClcStoneConfig.DA_StoneConfig 的 CrackCoverageRange 按 Seed 确定性 roll。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool Initialize(const FClcStoneRuntimeData& StoneData, int32 DefectCount, float TargetCoverage,
		int32 VoxelResolution, const FString& ShellMaterialPath);

	/**
	 * 用世界空间固定刀口切割。函数内部把刀口平面换算到源 mesh 局部空间，
	 * 自动切走体积较小的一侧；bForceRemoveNegativeSide 仅供调试时强制切负侧。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool ExecuteCut(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
		bool bForceRemoveNegativeSide, int32& OutCutAwayTotal,
		int32& OutCutAwayJade, int32& OutCutAwayCrack);

	/** 当前刀口是否同时穿过剩余主体两侧。 */
	bool CanCutAtWorldPlane(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld) const;

	/** 源 mesh 包围盒沿指定世界轴投影后的半尺寸。 */
	float GetHalfExtentAlongWorldAxis(const FVector& WorldAxis) const;

	/** 获取当前石头运行时数据（写回 CutPlanes + 体积统计 + Phase）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool GetStoneData(FClcStoneRuntimeData& OutData) const;

	/** 上台时把包围盒最长轴自动转正到当前工作台的左右移动方向。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	void AutoAlignLongestAxis();

	/** 切走小块的世界位置（P6 飞金币动效用；返回切平面与剩余体边界的交点近似）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	FVector GetCutPieceWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	const FClcStoneVoxelField3D& GetVoxelField() const { return VoxelField; }

	/** 供出售台/外部旋转展示用——返回 PMC，可 SetWorldRotation。 */
	UPrimitiveComponent* GetDisplayMesh() const;

	/** 讨价还价锁价（出售台调用） */
	void MarkHaggleResolved(int32 LockedPrice);

	/** 是否已讨价锁价 */
	bool IsHaggleResolved() const { return CachedStoneData.bHaggleResolved; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* CutMesh;

	UPROPERTY()
	UMaterialInstanceDynamic* ShellMID;

private:
	/** 当前石头数据（运行时持续更新切平面与统计） */
	FClcStoneRuntimeData CachedStoneData;

	/** 3D 体素场（不序列化；从 Seed 重生成 + 回放 CutPlanes 重建） */
	FClcStoneVoxelField3D VoxelField;

	/** 源 mesh（读 LOD0 三角形用，不直接渲染） */
	UPROPERTY()
	TObjectPtr<UStaticMesh> SourceMesh;

	/** 切平面记录（局部空间，重建 PMC 时按 bRemovedNegative 保留另一侧） */
	TArray<FClcCutPlaneRecord> CutPlanes;

	bool bInitialized = false;

	/** 最近一次切走侧的世界位置缓存（GetCutPieceWorldLocation 用） */
	FVector LastCutPieceWorldCenter = FVector::ZeroVector;

	/** 切面材质路径（P6 新增，空则退化为顶点色模式） */
	FString CutFaceMaterialPath;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CutFaceMID;

	// ---- mesh 重建 ----

	/** 回放全部已记录的切平面到 PMC（用引擎 SliceProceduralMesh） */
	void ReplayAllCuts();

	/** 对指定 section 的顶点用体素场采样写入顶点色 */
	void ApplyVoxelColorsToSection(int32 SectionIndex);

	/** 世界刀口平面 → 源 mesh 局部平面。 */
	bool BuildLocalCutPlane(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
		FVector& OutPlaneNormal, float& OutPlaneDistance) const;

	/** 切面 cap 顶点颜色：按体素场采样（玉=绿/裂=红/杂=黄/废=灰）——P2 验证对齐用 */
	FColor SampleVoxelColor(const FVector& LocalPos) const;
};
