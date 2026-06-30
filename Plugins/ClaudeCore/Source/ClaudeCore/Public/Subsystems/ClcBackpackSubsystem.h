// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Interfaces/ClcStoneCarrier.h"
#include "Data/ClcJadeTypes.h"
#include "ClcBackpackSubsystem.generated.h"

class UClcBackpackWidget;

UCLASS()
class CLAUDECORE_API UClcBackpackSubsystem : public ULocalPlayerSubsystem, public IClcStoneCarrier
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- IClcStoneCarrier (pure virtual) ----
	virtual TArray<FClcStoneRuntimeData> GetStones() const override;
	virtual int32 AddStone(const FClcStoneRuntimeData& StoneData) override;
	virtual bool RemoveStone(int32 StoneIndex) override;
	virtual int32 GetGold() const override;
	virtual void AddGold(int32 Amount) override;
	virtual bool SpendGold(int32 Amount) override;

	FClcStoneRuntimeData* GetStoneMutable(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void UpdateStoneData(int32 Index, const FClcStoneRuntimeData& UpdatedData);

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void ToggleBackpack();

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	bool IsBackpackOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	UClcBackpackWidget* GetBackpackWidget() const { return BackpackWidget; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	int32 GetGoldValue() const { return Gold; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	int32 GetTotalEarned() const { return TotalEarned; }

	/** GM命令：增加金币（按~打开控制台输入 AddGold 50000） */
	UFUNCTION(Exec, BlueprintCallable, Category = "ClcBackpack")
	void GMAddGold(int32 Amount);

	/** 蓝图接口：增加金币 */
	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void AddGoldBP(int32 Amount) { AddGold(Amount); }

private:
	void ShowNotification(const FString& Message);

	TArray<FClcStoneRuntimeData> Stones;
	int32 Gold = 0;

	static constexpr int32 MAX_STONE_SLOTS = 200;

	UPROPERTY()
	UClcBackpackWidget* BackpackWidget;

	UPROPERTY()
	TSubclassOf<UClcBackpackWidget> BackpackWidgetClass;

	bool bIsOpen = false;
	int32 TotalEarned = 0;
};
