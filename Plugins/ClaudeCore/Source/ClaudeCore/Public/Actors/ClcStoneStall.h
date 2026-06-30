// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcStoneStall.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UClcStoneMarketSubsystem;
class AClcStone;

/**
 * 摊位——摆放多块原石，鹰眼能量球在此生成
 */
UCLASS()
class CLAUDECORE_API AClcStoneStall : public AActor
{
	GENERATED_BODY()

public:
	AClcStoneStall();

	// 在编辑器中修改属性时触发——预览摊位尺寸和格子布局
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	void SpawnStones();

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	FTransform GetBallSpawnLocation() const;

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	const TArray<AClcStone*>& GetDisplayedStones() const { return SpawnedStones; }

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	float GetTotalTheoreticalValue() const;

protected:
	virtual void BeginPlay() override;

	/** 摊位根组件（无缩放）——Actor Scale 保持 (1,1,1)，桌面缩放设在 StallMesh 上避免污染 Actor Transform */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	USceneComponent* BenchRoot;

	/** 摊位显示Mesh（可替换，挂在 BenchRoot 下，桌面缩放在此设置） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	UStaticMeshComponent* StallMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	USceneComponent* BallSpawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	USceneComponent* StoneSpawnCenter;

	/** 编辑器预览用的格子占位 InstancedMesh */
	UPROPERTY(Transient, DuplicateTransient)
	class UInstancedStaticMeshComponent* PreviewGrid;

private:
	UPROPERTY()
	TArray<AClcStone*> SpawnedStones;

	UPROPERTY()
	UClcStoneMarketSubsystem* MarketSubsystem;

	/** 计算网格行列（Cols/Rows），返回是否有效 */
	bool CalcGridLayout(int32 Count, int32& OutCols, int32& OutRows) const;

	/** 编辑器里刷新预览（摊位Mesh尺寸 + 格子占位） */
	void RefreshEditorPreview();

	/** 给 PreviewGrid 添加格子实例 */
	void BuildGridPreview(int32 Cols, int32 Rows, float CellSize);
};
