// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcStallConfig.generated.h"

/**
 * 摊位配置
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcStallConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 每个摊位生成的石头数量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall")
	int32 StonesPerStall = 6;

	/** 单元格子边长（石头直径上限） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "50.0", ClampMax = "500.0"))
	float UnitCellSize = 120.0f;

	/** 摊位 mesh 在格子区域外的外扩留白（单位：格子尺寸的倍数，1 = 外扩一个格子宽度） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float MarginCells = 0.5f;

	/** 石头缩放随机范围（会被 UnitCellSize 硬约束） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall")
	FVector2D StoneScaleRange = FVector2D(0.6f, 1.0f);

	/** 格子内位置微扰比例（0 = 居中，0.1 = ±10%） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CellJitterRatio = 0.1f;

	/** 摊位 Mesh 自动适配网格的厚度比例（Z 缩放） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall")
	float StallThicknessScale = 0.2f;

	/** 石头交互半径——玩家进入此距离显示外圈并参与瞄准判定（原 AClcStone 构造硬编码 400，挪到 DA 统一调） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "50.0", ClampMax = "2000.0"))
	float StoneInteractionRadius = 400.0f;

	/** 瞄准球扫半径——把摄像机细射线扩成球扫放宽命中；0 = 退回细射线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stall", meta = (ClampMin = "0.0"))
	float StoneAimSweepRadius = 25.0f;
};
