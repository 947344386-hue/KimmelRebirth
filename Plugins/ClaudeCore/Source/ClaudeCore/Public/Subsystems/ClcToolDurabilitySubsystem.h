// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tools/ClcStoneTool.h"
#include "ClcToolDurabilitySubsystem.generated.h"

struct FClcSaveData;

/**
 * 工具耐久持久化子系统 —— 跨工作台会话保存每种工具的当前耐久。
 *
 * 生命周期：
 *   - Initialize：遍历所有工具类型，默认耐久 = Max
 *   - 工具 ConsumeDurability → SetDurability 写回本子系统
 *   - 工具 Initialize → GetDurability 读取（如果未存储过则返回 Max）
 *   - 修理站 RestoreDurability → 重置为 Max
 *
 * 单例获取：UClcToolDurabilitySubsystem::Get(World)
 */
UCLASS()
class CLAUDECORE_API UClcToolDurabilitySubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 从 World 获取本子系统（便捷方法） */
	static UClcToolDurabilitySubsystem* Get(const UWorld* World);

	/**
	 * 注册工具类型与其最大耐久——由工具 Spawn 时调用。
	 * 若该类型尚无存储耐久，则用 MaxDurability 初始化（满耐久）。
	 * Subsystem 不硬编码 max，完全由工具实例的 MaxDurability（BP 可配）驱动。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	void InitTool(EClcRepairableTool ToolType, float MaxDurability);

	/** 获取工具当前耐久（未存储过则返回该类型注册的 MaxDurability） */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	float GetDurability(EClcRepairableTool ToolType) const;

	/** 写入工具当前耐久；工具类型尚未注册时忽略 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	void SetDurability(EClcRepairableTool ToolType, float Value);

	/** 恢复指定工具到满耐久；工具类型尚未注册时忽略 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	void RestoreDurability(EClcRepairableTool ToolType);

	/** 按位掩码批量恢复耐久（Bitmask：Opener=1|Flashlight=2|Combined=4|Blade=8） */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	void RestoreDurabilityMask(int32 Mask);

	// ---- 升级所有权（运行时内存，与耐久同生命周期；跨工作台会话保持，不跨游戏重启） ----

	/** 玩家是否拥有指定升级 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	bool OwnsUpgrade(EClcToolUpgrade Upgrade) const;

	/** 授予升级；已拥有则返回 false 不重复授予 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	bool GrantUpgrade(EClcToolUpgrade Upgrade);

	/** 便捷封装：是否拥有「手电擦石器」组合工具升级 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	bool HasCombinedTool() const { return OwnsUpgrade(EClcToolUpgrade::CombinedTool); }

	/** 便捷封装：是否拥有「解石台」升级 */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	bool HasCuttingTable() const { return OwnsUpgrade(EClcToolUpgrade::CuttingTable); }

	/** 是否需要修复（耐久 < 最大值） */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	bool NeedsRepair(EClcRepairableTool ToolType) const;

	/** 获取工具类型的最大耐久（从注册值读，未注册返回硬编码兜底） */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	float GetMaxDurability(EClcRepairableTool ToolType) const;

	/** 耐久比例（0~1，用于 UI） */
	UFUNCTION(BlueprintCallable, Category = "ClcToolDurability")
	float GetDurabilityRatio(EClcRepairableTool ToolType) const;

	// ---- 存档序列化 ----

	/** 从存档数据恢复到 Subsystem */
	void RestoreFromSaveData(const struct FClcSaveData& Data);

private:
	/** 每种工具的当前耐久 */
	TMap<EClcRepairableTool, float> DurabilityStore;

	/** 每种工具的最大耐久（由工具 Spawn 时注册，BP 可配） */
	TMap<EClcRepairableTool, float> MaxDurabilityStore;

	/** 已购买的升级集合 */
	TSet<EClcToolUpgrade> OwnedUpgrades;
};