// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcEagleEyeConfig.generated.h"

class AActor;
class UMaterialInterface;

/** 鹰眼响应类型 */
UENUM(BlueprintType)
enum class EClcEagleEyeResponseMode : uint8
{
	SeeThrough UMETA(DisplayName = "可看穿（下→上）"),
	Occluded   UMETA(DisplayName = "不可看穿（上→下）")
};

/** 一类 Actor 的鹰眼默认响应规则；Actor Tag 可覆盖单个实例。 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcEagleEyeResponseRule
{
	GENERATED_BODY()

	/** 响应该规则的 Actor 类（包含其子类）；更具体的类应排在数组前面。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay", meta = (AllowAbstract = "false"))
	TSubclassOf<AActor> ActorClass;

	/** 默认响应类型；单个实例可用 EagleEyeSeeThrough / EagleEyeOccluded Actor Tag 覆盖。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	EClcEagleEyeResponseMode Mode = EClcEagleEyeResponseMode::Occluded;
};

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

	/** 是否启用鹰眼 Mesh 扫描层 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	bool bEnableXRayScan = true;

	/** 响应 Actor 规则。数组顺序即匹配优先级；留空时兼容旧行为，仅扫描商人且可看穿。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay", meta = (TitleProperty = "ActorClass"))
	TArray<FClcEagleEyeResponseRule> ResponseRules;

	/** 可看穿扫描材质（下→上）。留空时使用 ScanFX 的 MI_ScanFX_TriangleScanner。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	TSoftObjectPtr<UMaterialInterface> XRayScanMaterial;

	/** 不可看穿扫描材质（上→下）。材质须支持 Scan Box Origin / Size 参数并启用深度测试。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EagleEye|XRay")
	TSoftObjectPtr<UMaterialInterface> OccludedScanMaterial;
};
