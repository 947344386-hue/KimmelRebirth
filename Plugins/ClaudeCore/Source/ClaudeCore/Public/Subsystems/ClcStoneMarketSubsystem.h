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

	/** 生成一块石头的内部数据（不创建Actor） */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	FClcStoneInternalData GenerateStoneInternal(bool& bOutSuccess);

	/** 生成石头展示名 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	FString GenerateDisplayName(const FString& Origin) const;

	/** V2定价公式——给定石头运行时数据，返回当前回收价格 */
	UFUNCTION(BlueprintCallable, Category = "ClcMarket")
	int32 CalculateSalePrice(const FClcStoneRuntimeData& StoneData) const;

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

private:
	/** 掷种水档位——带产地软关联 */
	EClcJadeGrade RollGrade(FRandomStream& Random, const FString& Origin) const;

	/** 生成绿/黑比例和最大连续块比例 */
	void RollRatios(FRandomStream& Random, float& OutGreen, float& OutBlack, float& OutLargestPatch) const;

	/** 计算购买标价（含隐藏溢价） */
	int32 CalculatePurchasePrice(const FClcStoneInternalData& Data) const;

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
