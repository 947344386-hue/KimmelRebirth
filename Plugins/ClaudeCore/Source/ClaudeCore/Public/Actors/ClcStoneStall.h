// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcMerchantTypes.h"
#include "ClcStoneStall.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UArrowComponent;
class UClcStoneMarketSubsystem;
class AClcStone;
class AClcMerchant;

/** 石头被买走时广播——摊位算好购买结果（买走好的/烂的）一并传出 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStoneRemoved, EClcPurchaseOutcome, Outcome);

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

	/** 石头被买走时广播 */
	UPROPERTY(BlueprintAssignable, Category = "ClcStall")
	FOnStoneRemoved OnStoneRemoved;

	/** 获取绑定的商人（可能为空） */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	AClcMerchant* GetMerchant() const { return SpawnedMerchant; }

	/** 购买流程调用——石头即将被销毁前通知摊位，摊位算好购买结果并广播 */
	void NotifyStoneRemoved(AClcStone* Stone);

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

	/** 商人站位锚点——箭头朝向 = 商人面朝方向，每个实例可独立编辑 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* MerchantSpawnPoint;

	/** 编辑器预览用的格子占位 InstancedMesh */
	UPROPERTY(Transient, DuplicateTransient)
	class UInstancedStaticMeshComponent* PreviewGrid;

private:
	UPROPERTY()
	TArray<AClcStone*> SpawnedStones;

	UPROPERTY()
	AClcMerchant* SpawnedMerchant = nullptr;

	UPROPERTY()
	UClcStoneMarketSubsystem* MarketSubsystem;

	/** 计算网格行列（Cols/Rows），返回是否有效 */
	bool CalcGridLayout(int32 Count, int32& OutCols, int32& OutRows) const;

	/** 编辑器里刷新预览（摊位Mesh尺寸 + 格子占位） */
	void RefreshEditorPreview();

	/** 给 PreviewGrid 添加格子实例 */
	void BuildGridPreview(int32 Cols, int32 Rows, float CellSize);

	/** 生成商人 NPC 并绑定自身 */
	void SpawnMerchant();
};
