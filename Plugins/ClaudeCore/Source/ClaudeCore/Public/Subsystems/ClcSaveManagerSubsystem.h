// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ClcSessionTypes.h"
#include "ClcSaveManagerSubsystem.generated.h"

/**
 * 存档管理子系统 —— GameInstanceSubsystem。
 *
 * 槽位两类：
 *  - AutoSave：自动保存槽（1 个），定时/事件触发自动覆盖
 *  - ManualSlot_0 ~ ManualSlot_N-1：手动槽位（默认 5 个），玩家在 2 级 UI 中选择写入/读取
 */
UCLASS()
class CLAUDECORE_API UClcSaveManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static const FString AutoSaveSlotName;

	/** 手动槽位名前缀（ManualSlot_0 ~ ManualSlot_{Max-1}） */
	static const FString ManualSlotPrefix;

	/** 组装手动槽位名 */
	static FString MakeManualSlotName(int32 Index);

	// ---- 槽位查询 ----

	/** 获取所有存档槽位元数据（AutoSave + 所有手动槽位） */
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	TArray<FClcSaveMetaData> GetSaveSlots() const;

	/** 是否有任何存档存在（AutoSave 或任意手动槽位） */
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool HasAnySave() const;

	/** 获取手动槽位数上限 */
	UFUNCTION(BlueprintPure, Category = "ClcSave")
	int32 GetMaxSaveSlots() const { return MaxSaveSlots; }

	// ---- 保存/加载/删除 ----

	/** 保存到指定槽位（默认 AutoSave） */
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool SaveGame(const FString& SlotName = TEXT("AutoSave"));

	/** 从指定槽位加载（默认 AutoSave） */
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool LoadGame(const FString& SlotName = TEXT("AutoSave"));

	/** 删除指定槽位 */
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool DeleteSave(const FString& SlotName);

	// ---- 自动保存 ----

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	void TriggerAutoSave();

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	void SetAutoSaveEnabled(bool bEnabled);

	// ---- 状态 ----

	const FString& GetCurrentSlot() const { return CurrentSlot; }
	void SetCurrentSlot(const FString& SlotName) { CurrentSlot = SlotName; }

	// ---- 通知（由 BackpackSubsystem 调用） ----

	void NotifyGoldChanged(int32 NewGold);

private:
	FClcSaveData CollectSaveData() const;
	bool DistributeSaveData(const FClcSaveData& Data);
	bool WriteSaveFile(const FString& SlotName, const FClcSaveData& Data);
	bool ReadSaveFile(const FString& SlotName, FClcSaveData& OutData) const;
	bool ValidateSaveData(const FClcSaveData& Data) const;
	/** 读元数据；坏档返回 false，OutMeta 保持默认 */
	bool ReadMetaData(const FString& SlotName, FClcSaveMetaData& OutMeta) const;
	void RestartAutoSaveTimer();

	/** 保存/加载失败时弹 Toast 提示玩家；LocalPlayer 为空（Shutdown）时静默跳过 */
	void NotifySaveFailedToast(const FString& SlotName, const FString& Reason) const;

	// ---- 配置 ----

	/** 自动保存间隔（秒，默认 300 = 5 分钟） */
	UPROPERTY(EditDefaultsOnly, Category = "ClcSave")
	float AutoSaveIntervalSeconds = 300.0f;

	/** 金币增量触发自动保存的阈值（默认 5000） */
	UPROPERTY(EditDefaultsOnly, Category = "ClcSave")
	int32 AutoSaveGoldDeltaThreshold = 5000;

	/** 手动槽位数上限（默认 5，玩家手动存档可选槽位） */
	UPROPERTY(EditDefaultsOnly, Category = "ClcSave", meta = (ClampMin = "1"))
	int32 MaxSaveSlots = 5;

	// ---- 运行时状态 ----

	FString CurrentSlot;
	int32 LastAutoSavedGold = 0;
	double LastAutoSaveTime = 0.0;
	/** 累计游戏时长（秒），跨存档持久化在 SaveData.PlayTimeHours */
	float AccumulatedPlayTimeSeconds = 0.0f;
	bool bAutoSaveEnabled = true;
	FTimerHandle AutoSaveTimerHandle;
};
