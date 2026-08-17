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
 * 解石台上的石头载体。用 UProceduralMeshComponent 承载可切外壳：
 *   section 0 = 外壳三角形（M_StoneShell，保留原 UV）；
 *   section 1+ = 切面 cap（M_StoneCutFace，VertexColor.G 在玉/杂两套 PBR 间 lerp，贴图由 DA_JadeTextureConfig 注入）。
 *
 * 内部品质由 FClcStoneVoxelField3D 承载（从 InternalData.Seed 懒生成，与 2D 分布图独立——
 * 因阶段互斥两者不在同一块石头共存）。切平面由解石台传入世界空间刀口点 + MovementAxisWorld，
 * BuildLocalCutPlane 换算到局部空间后用 UKismetProceduralMeshLibrary::SliceProceduralMesh 切割，
 * 不再手写三角形裁剪/cap 三角化（已由引擎 FGeomTools 承担）。
 *
 * 每次 ExecuteCut：体素场 ComputeSliceCounts 算两侧计数 → 自动切走较小侧（或调用方 force）→
 * ApplyCut 更新 RemoveMask → SliceProceduralMesh 切 PMC（bCreateOtherHalf=true，切下块缓存到 LastOtherHalf
 * 供解石台开物理掉落）→ ApplyVoxelColorsToSection 给 cap 写顶点色 + planar UV。
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
		int32& OutCutAwayJade, int32& OutCutAwayCrack, int32& OutCutAwayImpurity);

	/** 当前刀口是否同时穿过剩余主体两侧。 */
	bool CanCutAtWorldPlane(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld) const;

	/**
	 * 预判切割将切走哪一侧（不修改体素场/mesh，供调用方选相机+决定 force 方向）。
	 * 返回 true 表示预判成功。OutRemoveNegative=true 表示将切走负侧。
	 * 与 ExecuteCut 的自动切较小侧逻辑一致；调用后立即 ExecuteCut 且中间不移动石头即可保证一致。
	 */
	bool PredictCutSide(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
		bool& OutRemoveNegative) const;

	/**
	 * 预判当前刀口将切走多少比例的原石总体积（不修改体素场/mesh）。
	 * OutRatio ∈ [0,1] = 切走侧(较小侧)体素数 / 原石总体素数。
	 * 供 HUD 实时显示"切块过大/过小/标准"四态。与 PredictCutSide 同源（复用 ComputeSliceCounts）。
	 * 返回 false 表示刀口未同时穿过两侧或未初始化。
	 */
	bool PredictCutRatio(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
		float& OutRatio) const;

	/** 原石总体素数（Initialize 时缓存，不随切割变化，作尺寸比例基准）。 */
	int32 GetTotalVoxels() const { return TotalVoxels; }

	/** 源 mesh 包围盒沿指定世界轴投影后的半尺寸。 */
	float GetHalfExtentAlongWorldAxis(const FVector& WorldAxis) const;

	/** 获取当前石头运行时数据（写回 CutPlanes + 体积统计 + Phase）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool GetStoneData(FClcStoneRuntimeData& OutData) const;

	/** 上台时把包围盒最长轴自动转正到当前工作台的左右移动方向。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	void AutoAlignLongestAxis();

	/** 切走小块的世界位置（飞金币动效用；返回切平面与剩余体边界的交点近似）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	FVector GetCutPieceWorldLocation() const;

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingStone")
	const FClcStoneVoxelField3D& GetVoxelField() const { return VoxelField; }

	/** 供出售台/外部旋转展示用——返回 PMC，可 SetWorldRotation。 */
	UPrimitiveComponent* GetDisplayMesh() const;

	/**
	 * 返回最近一次 ExecuteCut 由 SliceProceduralMesh 产生的 OtherHalf PMC（切下块几何，可能为空）。
	 * 调用方负责注册+开物理+清理。ExecuteCut 内部仅缓存，不再丢弃。
	 */
	UProceduralMeshComponent* GetLastOtherHalf() const { return LastOtherHalf.Get(); }

	/** 调用方取走切下块并接管其生命周期后，清掉石头侧的弱引用，避免双份持有。 */
	void ClearLastOtherHalf() { LastOtherHalf.Reset(); }

	/** 讨价还价锁价（出售台调用） */
	void MarkHaggleResolved(int32 LockedPrice);

	/** 是否已讨价锁价 */
	bool IsHaggleResolved() const { return CachedStoneData.bHaggleResolved; }

	/**
	 * 本刀切走侧玉肉体素的包围盒（mesh 局部空间）。
	 * ExecuteCut 内从切走侧 JadeBody 体素算出并缓存，供调用方传入 CalculateCutPieceValue 计算紧凑度。
	 */
	FBox GetLastCutJadeBoundingBox() const { return LastCutJadeBounds; }

	/**
	 * 以体素场为权威重算累计切走体积、剩余总体积与剩余玉肉体积，
	 * 并写回 CachedStoneData。初始化回放后和每次 ApplyCut 后调用。
	 */
	void RefreshCutStatistics();

	/**
	 * 切石结算——由解石台在每刀金币计算后调用，让石头 Actor 自己更新
	 * ConsumedCutBudget 和 TotalSettledValue，解决解石台局部副本覆盖丢失问题。
	 */
	void ApplyCutSettlement(int32 ConsumedBudgetAfter, int32 PieceGold);

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

	/** 原石总体素数（Initialize 生成体素场后、回放切平面之前缓存；作切块尺寸比例基准，不随切割变化） */
	int32 TotalVoxels = 0;

	/** 原石玉肉体素数（Initialize 生成体素场后、回放切平面之前缓存；供预算分摊用） */
	int32 OriginalJade = 0;

	/** 原石裂纹体素数（仅展示/审计，不参与货币公式） */
	int32 OriginalCrack = 0;

	/** 原石杂质体素数（仅展示/审计，不参与货币公式） */
	int32 OriginalImpurity = 0;

	/** 最近一次切走侧玉肉体素的包围盒（mesh 局部空间，供紧凑度计算） */
	FBox LastCutJadeBounds = FBox(ForceInit);

	/** 最近一次切走侧的世界位置缓存（GetCutPieceWorldLocation 用） */
	FVector LastCutPieceWorldCenter = FVector::ZeroVector;

	/** 最近一次 ExecuteCut 由 SliceProceduralMesh 产生的切下块 PMC（供调用方开物理掉落） */
	UPROPERTY()
	TWeakObjectPtr<UProceduralMeshComponent> LastOtherHalf;

	/** 切面材质路径（空则退化为顶点色模式） */
	FString CutFaceMaterialPath;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CutFaceMID;

	// ---- mesh 重建 ----

	/** 回放全部已记录的切平面到 PMC（用引擎 SliceProceduralMesh） */
	void ReplayAllCuts();

	/** 对指定 cap section 写顶点色 + planar UV（按切面显示法线投影）。
	 *  InteriorSampleDirectionWorld 指向该块内部，避免保留块与切下块采到切面的另一侧。
	 *  TargetMesh 默认 CutMesh（保留块）；切下块 OtherHalf 需显式传入。 */
	void ApplyVoxelColorsToSection(int32 SectionIndex, const FVector& SurfaceNormalWorld,
		const FVector& InteriorSampleDirectionWorld, UProceduralMeshComponent* TargetMesh = nullptr);

	/** 世界刀口平面 → 源 mesh 局部平面。 */
	bool BuildLocalCutPlane(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
		FVector& OutPlaneNormal, float& OutPlaneDistance) const;

	/** 切面 cap 顶点颜色：按体素场采样（玉=绿/裂=红/杂=黄/废=灰） */
	FColor SampleVoxelColor(const FVector& LocalPos) const;
};
