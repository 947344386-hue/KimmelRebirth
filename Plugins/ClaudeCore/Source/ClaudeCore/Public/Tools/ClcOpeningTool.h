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

	virtual void OnUpdate(const FClcToolTraceInfo& TraceInfo) override;
	virtual void OnLeftClick(bool bPressed) override;

protected:
	/** 打磨核心——Möller-Trumbore 射线-三角形求交 + GrindAtUV */
	bool ExecuteGrind(const FVector& RayOrigin, const FVector& RayDirection);

	// ---- 配置 ----

	/** 打磨时每秒消耗的耐久 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningTool|Config")
	float DurabilityPerSecond = 2.0f;

private:
	/** 是否正在打磨（左键按住中） */
	bool bIsGrinding = false;
};
