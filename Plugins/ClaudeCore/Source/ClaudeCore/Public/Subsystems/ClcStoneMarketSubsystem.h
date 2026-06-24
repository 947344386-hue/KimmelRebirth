// Copyright ClaudeCore. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneMarketSubsystem.generated.h"
class UClcStoneConfig; class UClcStoneMeshConfig; class UClcStallConfig; class AClcStoneStall; class AClcStone;
UCLASS()
class CLAUDECORE_API UClcStoneMarketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") void RefreshMarket();
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") FClcStoneInternalData GenerateStoneInternal(bool& bOutSuccess);
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") FString GenerateDisplayName(const FString& Origin) const;
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") int32 CalculateSalePrice(const FClcStoneRuntimeData& StoneData) const;
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") void RegisterStall(AClcStoneStall* Stall);
	UFUNCTION(BlueprintCallable, Category = "ClcMarket") void UnregisterStall(AClcStoneStall* Stall);
	const TArray<TWeakObjectPtr<AClcStoneStall>>& GetStalls() const { return RegisteredStalls; }
	UFUNCTION(BlueprintCallable) UClcStoneConfig* GetStoneConfig() const { return StoneConfig; }
	UFUNCTION(BlueprintCallable) UClcStoneMeshConfig* GetMeshConfig() const { return MeshConfig; }
	UFUNCTION(BlueprintCallable) UClcStallConfig* GetStallConfig() const { return StallConfig; }
	UFUNCTION(BlueprintCallable) float CalculateTheoreticalValue(const FClcStoneInternalData& Data) const;
private:
	EClcJadeGrade RollGrade(FRandomStream& Random, const FString& Origin) const;
	void RollRatios(FRandomStream& Random, float& OutGreen, float& OutBlack, float& OutLargestPatch) const;
	int32 CalculatePurchasePrice(const FClcStoneInternalData& Data) const;
	UPROPERTY() UClcStoneConfig* StoneConfig;
	UPROPERTY() UClcStoneMeshConfig* MeshConfig;
	UPROPERTY() UClcStallConfig* StallConfig;
	TArray<TWeakObjectPtr<AClcStoneStall>> RegisteredStalls;
	TMap<FString, int32> DisplayNameCounters;
};