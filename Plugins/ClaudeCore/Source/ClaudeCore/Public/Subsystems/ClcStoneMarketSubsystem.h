// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneMarketSubsystem.generated.h"

class UClcStoneConfig;
class UClcStoneMeshConfig;
class UClcStallConfig;
class UClcShellTextureConfig;
class AClcStoneStall;
class AClcStone;

/**
 * 石头市场子系统——管理石头生成、定价、刷新
 */
UCLASS()
class CLAUDECORE_API UClcStoneMarketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 刷新全市场石头（销毁旧石头、按配置重新生成所有摊位） */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	void RefreshMarket();

	/** 生成一块石头的内部数据（不创建Actor）。DeceptionLevel=商人欺骗倾向，用于 roll 声称种水黑话 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	FClcStoneInternalData GenerateStoneInternal(bool& bOutSuccess, float DeceptionLevel = 0.5f);

	/** 生成石头展示名：产地+皮壳+重量+黑话吹卖句 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	FString GenerateDisplayName(const FClcStoneInternalData& StoneData) const;

	/** V2定价公式——给定石头运行时数据，返回当前回收价格 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculateSalePrice(const FClcStoneRuntimeData& StoneData) const;

	/**
	 * 解石切块折金币——体积驱动价值公式。
	 * CutAwayTotal/Jade/Crack = 切走侧体素数；VoxelVolume = 单体素体积(cm³)。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculateCutPieceValue(int32 CutAwayTotal, int32 CutAwayJade, int32 CutAwayCrack,
		float VoxelVolume, EClcJadeGrade Grade) const;

	/** 解石剩余主体回收估值——已露截面体积外推到未切体积（仿 2D 净外推模型）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculateCutStoneSalePrice(const FClcStoneRuntimeData& StoneData) const;

	/**
	 * 讨价还价最终价——基于参考价按对称赔率结算。
	 * 成功 → BasePrice × (1 + Ratio)；失败 → BasePrice × (1 - Ratio)。
	 * 集中价格数学，避免在 vendor/widget 里复制乘法。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculateHagglePrice(int32 BasePrice, float Ratio, bool bSuccess, bool bSymmetricFailure = true) const;

	/** 注册摊位 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	void RegisterStall(AClcStoneStall* Stall);

	/** 注销摊位 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	void UnregisterStall(AClcStoneStall* Stall);

	/** 获取所有已注册摊位（C++内部使用） */
	const TArray<TWeakObjectPtr<AClcStoneStall>>& GetStalls() const { return RegisteredStalls; }

	/** 获取配置 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	UClcStoneConfig* GetStoneConfig() const { return StoneConfig; }

	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	UClcStoneMeshConfig* GetMeshConfig() const { return MeshConfig; }

	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	UClcStallConfig* GetStallConfig() const { return StallConfig; }

	/** 获取皮壳纹理配置 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	UClcShellTextureConfig* GetShellTextureConfig() const { return ShellTextureConfig; }

	/** 计算一个石头的理论全开价值 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	float CalculateTheoreticalValue(const FClcStoneInternalData& Data) const;

	/** 计算购买标价（含隐藏溢价）——public 供 AClcStone 在 RecalculateSurfaceArea 后重算 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculatePurchasePrice(const FClcStoneInternalData& Data) const;

	/**
	 * 聚合背包悬浮 tips 数据——一次性算好名称/产地/皮壳或种水/当前价/购入价。
	 * BP 侧 StoneEntry 的 OnMouseEnter 调此函数，拿 FClcStoneTooltipInfo 直接渲染。
	 * 皮壳/种水互斥：OpenedGreenArea>0 → 显示种水；否则显示皮壳。
	 */
	UFUNCTION(BlueprintPure, Category = "ClcMarket")
	FClcStoneTooltipInfo BuildTooltipInfo(const FClcStoneRuntimeData& StoneData) const;

	/**
	 * 调试用：生成 Count 块石头，打印每块目标 vs 实际（玉/杂质/裂）占比、最大玉肉块占比，
	 * 并模拟全开校验 CalculateSalePrice(全开) ≈ CalculateTheoreticalValue（容差内）。
	 * 用于验证分布算法的统计收敛。PIE/编辑器任意调用一次看输出日志。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket|Debug")
	void DebugValidateGeneration(int32 Count = 20);

private:
	/** 掷种水档位——带产地软关联 */
	EClcJadeGrade RollGrade(FRandomStream& Random, const FString& Origin) const;

	// ---- 配置引用 ----
	UPROPERTY()
	UClcStoneConfig* StoneConfig;

	UPROPERTY()
	UClcStoneMeshConfig* MeshConfig;

	UPROPERTY()
	UClcStallConfig* StallConfig;

	UPROPERTY()
	UClcShellTextureConfig* ShellTextureConfig;

	// ---- 注册的摊位 ----
	TArray<TWeakObjectPtr<AClcStoneStall>> RegisteredStalls;

	/** 石头展示名计数器 */
	TMap<FString, int32> DisplayNameCounters;
};
