// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ClcStoneTool.h"
#include "ClcOpeningTool.generated.h"

/**
 * 开窗器工具——打磨去除皮壳，暴露下方玉/裂纹。
 *
 * 逻辑层（本类）：
 *   - OnUpdate：每帧把工具 Mesh 定位到命中点，垂直于表面
 *   - 左键按住：执行 Möller-Trumbore 射线-三角形求交 → 取 UV → 调 Stone->GrindAtUV
 *   - 耐久：打磨中按秒消耗
 *
 * 表现层（BP 子类实现，后续单独做）：
 *   - ToolMesh 的 StaticMesh（角磨机、砂轮等模型）
 *   - 打磨动画（OnLeftClick → BP 播放 Timeline / AnimMontage）
 *   - 打磨粒子特效、音效
 */
UCLASS(Blueprintable, BlueprintType)
class CLAUDECORE_API AClcOpeningTool : public AClcStoneTool
{
	GENERATED_BODY()

public:
	AClcOpeningTool();

	/** 左键打磨开始/结束时广播（bPressed=true=开始打磨，false=停止）。蓝图绑它驱动粒子/烟雾开关 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrindingStateChanged, bool, bGrinding);
	UPROPERTY(BlueprintAssignable, Category = "ClcOpeningTool")
	FOnGrindingStateChanged OnGrindingStateChanged;

	virtual void OnUpdate(const FClcToolTraceInfo& TraceInfo) override;
	virtual void OnLeftClick(bool bPressed) override;

	/** 增减开窗半径（由工作台 -/= 按键转发调用，正=增大，负=缩小） */
	UFUNCTION(BlueprintCallable, Category = "OpeningTool")
	void AdjustBrushRadius(float Delta);

	// ---- 配置：笔刷尺寸 ----

	/** 开窗半径下限（UV 空间 0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Brush", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float BrushRadiusMin = 0.01f;

	/** 开窗半径上限 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Brush", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float BrushRadiusMax = 0.25f;

	/** 每按一次 -/= 增减的半径量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Brush", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float BrushIncrementPerPress = 0.01f;

	/** 预览环的世界缩放倍率（UV半径×此值=世界单位半径） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Brush")
	float BrushScaleMultiplier = 100.0f;

	/** 预览环材质——白/雪白发光环，半透明 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Brush")
	UMaterialInterface* PreviewMaterial = nullptr;

protected:
	/** 打磨核心——Möller-Trumbore 射线-三角形求交 + GrindAtUV */
	bool ExecuteGrind(const FVector& RayOrigin, const FVector& RayDirection);

	/** 更新预览环的缩放和可见性 */
	void UpdatePreviewCylinder();

	// ---- 组件 ----

	/** 预览环——扁圆柱体，半径=开窗笔刷半径，附着在 ToolRoot 上。左键按下时显示 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PreviewCylinder;

	// ---- 配置 ----

	/** 打磨时每秒消耗的耐久 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Config")
	float DurabilityPerSecond = 2.0f;

private:
	/** 是否正在打磨（左键按住中） */
	bool bIsGrinding = false;
};
