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

	/** 从存档槽位数据还原石头（不重新掷，保留价格封顶结果） */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	void RestoreFromSlots(const TArray<struct FClcSlotSaveState>& Slots);

	/** 收集当前摊位上所有石头为存档槽位数据 */
	void CollectSlots(TArray<struct FClcSlotSaveState>& OutSlots) const;

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	const TArray<AClcStone*>& GetDisplayedStones() const { return SpawnedStones; }

	// ---- 价格封顶 & 换批 ----

	/** 重新生成石头 + 价格封顶（替换 StallZoneManager::RefreshAllStalls） */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	void RefreshAllStalls();

	/** 该摊位允许的最高购买价 = TargetPurchasePrice × PriceOverflowFactor */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	int32 GetMaxAllowedPrice() const;

	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	float GetTotalTheoreticalValue() const;

	/** 获取石头摆放中心的世界坐标（所有石头围绕此点）——商人 TalkTrigger 用此对齐中心 */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	FVector GetStoneSpawnCenterLocation() const;

	/** 石头被买走时广播 */
	UPROPERTY(BlueprintAssignable, Category = "ClcStall")
	FOnStoneRemoved OnStoneRemoved;

	/** 获取绑定的商人（可能为空） */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	AClcMerchant* GetMerchant() const { return SpawnedMerchant; }

	/** 购买流程调用——石头即将被销毁前通知摊位，摊位算好购买结果并广播 */
	void NotifyStoneRemoved(AClcStone* Stone);

	/** 获取摊位在关卡中的唯一 Id（供存档分组用——等于 GetPathName，无需手动配置） */
	UFUNCTION(BlueprintCallable, Category = "ClcStall")
	FName GetStallId() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 摊位根组件（无缩放）——Actor Scale 保持 (1,1,1)，桌面缩放设在 StallMesh 上避免污染 Actor Transform */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	USceneComponent* BenchRoot;

	/** 摊位显示Mesh（可替换，挂在 BenchRoot 下，桌面缩放在此设置） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStall")
	UStaticMeshComponent* StallMesh;

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

	/** 已售出石头的累计真实理论价值 + 数量——供摊位只剩最后一块时作比较基线 */
	float SumBoughtStoneValues = 0.0f;
	int32 BoughtStoneCount = 0;

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

	/** 单块石头 —— 若 PurchasePrice 超 MaxAllowedPrice，等比缩小石头尺寸压到区间内 */
	void ClampStonePriceIfNeeded(AClcStone* Stone);

	/** 对所有石头执行一次封顶（SpawnStones 末尾 + RefreshAllStalls 复用） */
	void ClampAllStones();

	// ---- 价格封顶配置（原 StallZoneManager 下放到每个摊位独立配置） ----

	/** 该摊位目标购买价（软上限，低于此值的石头不做处理） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ClcStall|Price", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 TargetPurchasePrice = 10000;

	/** 价格溢出系数 —— 硬上限 = Target × Factor。超出硬上限的石头会被等比缩小压价 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ClcStall|Price", meta = (AllowPrivateAccess = "true", ClampMin = "1.0", ClampMax = "5.0"))
	float PriceOverflowFactor = 1.3f;

	/** 缩小比例下限 —— 再贵也不能缩到看不见（0.6 = 缩小到 60% 为止） */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "ClcStall|Price", meta = (AllowPrivateAccess = "true", ClampMin = "0.1", ClampMax = "1.0"))
	float MinStoneScaleRatio = 0.6f;
};
