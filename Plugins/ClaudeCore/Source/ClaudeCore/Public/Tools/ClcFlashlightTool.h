// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ClcStoneTool.h"
#include "ClcFlashlightTool.generated.h"

class USpotLightComponent;

/**
 * 手电筒工具——光锥照射范围内皮壳半透明，透视下方玉/裂纹。
 *
 * 逻辑层（本类）：
 *   - OnUpdate：每帧把工具定位到命中点 + 法线方向悬浮一段距离
 *   - 左键按住：打开 SpotLight + 设置材质光锥参数（X-ray 透视效果）
 *   - 耐久：光照开启时按秒消耗
 *
 * 表现层（BP 子类实现，后续单独做）：
 *   - ToolMesh 的 StaticMesh（手电筒模型）
 *   - 开关灯音效、光锥特效
 */
UCLASS(Blueprintable, BlueprintType)
class CLAUDECORE_API AClcFlashlightTool : public AClcStoneTool
{
	GENERATED_BODY()

public:
	AClcFlashlightTool();

	virtual void Tick(float DeltaTime) override;
	virtual void OnUpdate(const FClcToolTraceInfo& TraceInfo) override;
	virtual void OnLeftClick(bool bPressed) override;
	virtual void OnDeactivated_Implementation() override;

	/** 由 Workbench 调用——覆盖手电配置 */
	UFUNCTION(BlueprintCallable, Category = "FlashlightTool")
	void ApplyConfig(float Intensity, float ConeAngle, float HoverHeight, float Range);

	/** 手电是否开启（HUD 查询） */
	UFUNCTION(BlueprintCallable, Category = "FlashlightTool")
	bool IsLightOn() const { return bLightOn; }

protected:
	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpotLightComponent* SpotLight;

	// ---- 配置 ----

	/** 光照强度（流明）—— 数值越大手电越亮，建议 2000~10000 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config", meta = (ClampMin = "0.0"))
	float FlashlightIntensity = 5000.0f;

	/** 光锥半角（度）—— 决定照射圆圈的大小，值越大覆盖面越广；材质的透视圆也按此角度同步生成 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config", meta = (ClampMin = "5.0", ClampMax = "80.0"))
	float FlashlightConeAngle = 25.0f;

	/** 光源悬浮高度（cm）—— SpotLight 离石头表面的距离，避免手电模型穿进石头 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float FlashlightHoverHeight = 15.0f;

	/** 光照有效范围（cm）—— 超出此距离光衰减为 0；需与 ConeAngle 配出的光圈半径匹配，否则会出现"光圈和透视圆对不齐" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float FlashlightRange = 80.0f;

	/** X-ray 最大透视强度（0=皮壳不透明, 1=完全透明露出玉石） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashlightXrayStrength = 0.65f;

	/** 每秒耐久消耗（灯开启时按秒扣减，耗尽自动关灯） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlashlightTool|Config")
	float DurabilityPerSecond = 1.0f;

private:
	/** 光锥是否开启（左键按住时为 true） */
	bool bLightOn = false;
};
