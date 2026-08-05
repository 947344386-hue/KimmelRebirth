// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ClcOpeningTool.h"
#include "ClcCombinedTool.generated.h"

class USpotLightComponent;

/**
 * 手电开窗器——开窗器 + 手电筒的组合升级工具。
 *
 * 继承 AClcOpeningTool 复用全部打磨/笔刷/预览贴画逻辑（左键打磨、滚轮调笔刷），
 * 额外叠加一个 SpotLight 与手电开关状态：
 *   - T 键（由 Workbench 调用 ToggleLight）开关手电，开灯时皮壳半透明 X-ray 透视
 *   - 打磨与开灯都从同一个耐久池消耗（ToolType=Combined 的独立持久化池）
 *
 * 工作台输入在 Combined 模式下：左键=打磨（继承）、滚轮=笔刷（继承）、T=开关灯（本类）。
 */
UCLASS(Blueprintable, BlueprintType)
class CLAUDECORE_API AClcCombinedTool : public AClcOpeningTool
{
	GENERATED_BODY()

public:
	AClcCombinedTool();

	virtual void Tick(float DeltaTime) override;
	virtual void OnUpdate(const FClcToolTraceInfo& TraceInfo) override;
	virtual void OnActivated_Implementation() override;
	virtual void OnDeactivated_Implementation() override;

	/** 由 Workbench（T 键）调用——翻转手电开关。耐久耗尽时强制关闭 */
	UFUNCTION(BlueprintCallable, Category = "ClcCombinedTool")
	void ToggleLight();

	/** 直接设置手电开关（false 时强制关灯） */
	UFUNCTION(BlueprintCallable, Category = "ClcCombinedTool")
	void SetLightOn(bool bOn);

	/** 手电是否开启（HUD / 补光查询） */
	UFUNCTION(BlueprintCallable, Category = "ClcCombinedTool")
	bool IsLightOn() const { return bLightOn; }

protected:
	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpotLightComponent* SpotLight;

	// ---- 手电配置（沿用 FlashlightTool 命名，BP 可覆写） ----

	/** 光照强度（流明） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight", meta = (ClampMin = "0.0"))
	float FlashlightIntensity = 5000.0f;

	/** 光锥半角（度）—— 决定照射圆圈大小，材质透视圆按此同步 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight", meta = (ClampMin = "5.0", ClampMax = "80.0"))
	float FlashlightConeAngle = 25.0f;

	/** 光源悬浮高度（cm）—— SpotLight 离石头表面的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float FlashlightHoverHeight = 15.0f;

	/** 光照有效范围（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float FlashlightRange = 80.0f;

	/** X-ray 最大透视强度（0=不透明, 1=完全透明） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashlightXrayStrength = 0.65f;

	/** 开灯时每秒耐久消耗 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcCombinedTool|Flashlight")
	float FlashlightDurabilityPerSecond = 1.0f;

private:
	/** 写 SpotLight 可见/强度 + 石头 MID 的 X-ray 参数 */
	void ApplyLightState();

	/** 把 FlashlightHoverHeight/ConeAngle/Range 等 UPROPERTY 同步到 SpotLight 组件
	 *  （位置/锥角/范围）——构造 + 每次激活时调用，让 BP 改 UPROPERTY 即生效 */
	void RefreshSpotLightConfig();

	/** 光锥是否开启 */
	bool bLightOn = false;
};
