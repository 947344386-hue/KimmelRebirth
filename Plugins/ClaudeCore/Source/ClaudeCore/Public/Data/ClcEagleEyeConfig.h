// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcEagleEyeConfig.generated.h"

class UMaterialInterface;

/**
 * 鹰眼能力配置
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcEagleEyeConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 每个商人被激活后的残留时长（秒）——per-merchant 独立计时，到点自然熄灭 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye", meta = (ClampMin = "0.0"))
	float ActiveDuration = 5.0f;

	/** 玩家按键冷却（秒）——防连点，CD 中按键无效；默认 0.3 仅防快速连点 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.3f;

	/** 范围判定半径（cm）——以玩家为中心的球半径，只激活此范围内的商人；同时也是扫描视觉半径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye", meta = (ClampMin = "0.0"))
	float ScanRadius = 1500.f;

	/** 扫描后处理材质（ScanFX 的 PPI_ScanWave_*） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|Scan Visual")
	TSoftObjectPtr<UMaterialInterface> ScanPostProcessMaterial;

	/** 扫描扩散时长（秒），写入材质 Scan Duration；材质按“当前游戏时间 - Scan Start Time”自行扩散并在此时长后淡出 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|Scan Visual", meta = (ClampMin = "0.0"))
	float ScanDuration = 2.0f;

	/** 是否在 ScanWave 上叠加 ScanFX XRay phantom 扫描层 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	bool bEnableXRayScan = true;

	/** XRay Scanner 蓝图类；默认使用 ScanFX 的 BP_ScanFX_XRay_Scanner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	TSoftClassPtr<AActor> XRayScannerClass;

	/** XRay Scanner Box 半尺寸（cm）；只捕获 EagleEyeXRay 专用碰撞代理的原石和商人 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay", meta = (ClampMin = "0.0"))
	FVector XRayBoxHalfExtents = FVector(1500.0f, 1500.0f, 500.0f);

	/** ScanDuration 结束后给 XRay Timeline/phantom 清理的额外时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay", meta = (ClampMin = "0.0"))
	float XRayCleanupGrace = 0.15f;
};
