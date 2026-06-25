// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcStoneStall.generated.h"
class USceneComponent; class UStaticMeshComponent; class UClcStoneMarketSubsystem; class AClcStone;
UCLASS()
class CLAUDECORE_API AClcStoneStall : public AActor
{
	GENERATED_BODY()
public:
	AClcStoneStall();
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(BlueprintCallable) void SpawnStones();
	UFUNCTION(BlueprintCallable) FTransform GetBallSpawnLocation() const;
	UFUNCTION(BlueprintCallable) const TArray<AClcStone*>& GetDisplayedStones() const { return SpawnedStones; }
	UFUNCTION(BlueprintCallable) float GetTotalTheoreticalValue() const;
protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* StallMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USceneComponent* BallSpawnPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) USceneComponent* StoneSpawnCenter;
	UPROPERTY(Transient, DuplicateTransient) class UInstancedStaticMeshComponent* PreviewGrid;
private:
	UPROPERTY() TArray<AClcStone*> SpawnedStones;
	UPROPERTY() UClcStoneMarketSubsystem* MarketSubsystem;
	bool CalcGridLayout(int32 Count, int32& OutCols, int32& OutRows) const;
	void RefreshEditorPreview();
	void BuildGridPreview(int32 Cols, int32 Rows, float CellSize);
};