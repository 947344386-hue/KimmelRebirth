// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ClcSessionTypes.h"
#include "ClcSaveManagerSubsystem.generated.h"

/**
 * 存档管理子系统 —— GameInstanceSubsystem。
 */
UCLASS()
class CLAUDECORE_API UClcSaveManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static const FString AutoSaveSlotName;

	// 槽位
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	TArray<FClcSaveMetaData> GetSaveSlots() const;

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool HasAnySave() const;

	// 保存/加载/删除
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool SaveGame(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool LoadGame(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	bool DeleteSave(const FString& SlotName);

	// 自动保存
	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	void TriggerAutoSave();

	UFUNCTION(BlueprintCallable, Category = "ClcSave")
	void SetAutoSaveEnabled(bool bEnabled);

	void NotifyGoldChanged(int32 NewGold);
	void NotifyTransactionCompleted();

	const FString& GetCurrentSlot() const { return CurrentSlot; }
	void SetCurrentSlot(const FString& SlotName) { CurrentSlot = SlotName; }

private:
	FClcSaveData CollectSaveData() const;
	void DistributeSaveData(const FClcSaveData& Data);
	bool WriteSaveFile(const FString& SlotName, const FClcSaveData& Data);
	bool ReadSaveFile(const FString& SlotName, FClcSaveData& OutData) const;
	FClcSaveMetaData ReadMetaData(const FString& SlotName) const;
	void RestartAutoSaveTimer();

	float AutoSaveIntervalSeconds = 300.0f;
	int32 AutoSaveGoldDeltaThreshold = 5000;
	FString CurrentSlot;
	int32 LastAutoSavedGold = 0;
	double LastAutoSaveTime = 0.0;
	bool bAutoSaveEnabled = true;
	FTimerHandle AutoSaveTimerHandle;
};
