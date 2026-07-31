// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcHaggleConfig.generated.h"

class UClcHaggleWidget;
class USkeletalMesh;
class UAnimSequence;

/** 讨价还价结算结果 */
UENUM(BlueprintType)
enum class EClcHaggleOutcome : uint8
{
	Cancelled = 0 UMETA(DisplayName = "取消"),
	Accepted  = 1 UMETA(DisplayName = "按参考价出手"),
	Success   = 2 UMETA(DisplayName = "加价成功"),
	Failure   = 3 UMETA(DisplayName = "加价失败")
};

/** 一档讨价还价幅度——上浮比例 + 对应 QTE 键数 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcHaggleTier
{
	GENERATED_BODY()

	/** 上浮比例（0.1=+10%）；失败按对称下折 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpliftRatio = 0.1f;

	/** 该档 QTE 键数（越多越难） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle", meta = (ClampMin = "1", ClampMax = "30"))
	int32 SequenceLength = 4;

	/** 显示名（如 "+10%"） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle")
	FText Label = FText::FromString(TEXT("+10%"));
};

/**
 * 讨价还价 QTE 配置（DA_HaggleConfig）。
 *
 * - HaggleTiers 留空时组件用内置默认 4 档（+10/20/30/50%，4/6/8/12 键）。
 * - 文案模板用 {0} 作价格占位符，运行时 FString::Format 替换。
 * - QTE 按键固定为 WASD（→ ↑/←/↓/→ 方向），未来扩展再加 QteKeys。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcHaggleConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 讨价还价档位（空=用内置默认 4 档） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle")
	TArray<FClcHaggleTier> HaggleTiers;

	/** 每个键的输入窗口（秒），超时即失败 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Timing", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float TimePerKey = 0.8f;

	/** 结果展示时长（秒）后自动完成售出；取消路径用短延时 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Timing", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float ResolveDelay = 1.2f;

	/** 是否对称下折：true→失败按 UpliftRatio 同比例下折；false→失败仍按参考价 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle")
	bool bSymmetricFailure = true;

	/** 可选 Widget 换皮类（空=用 C++ 默认布局） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|UI")
	TSubclassOf<UClcHaggleWidget> HaggleWidgetClass;

	// ---- NPC 文案模板（{0}=价格） ----

	/** NPC 报参考价的开场白 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText OfferLineTemplate = FText::FromString(TEXT("这块……最多 {0} 金。要不要谈谈？"));

	/** 「直接出手」按钮文案 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText AcceptLabel = FText::FromString(TEXT("直接出手 (空格)"));

	/** 加价成功 NPC 反应（{0}=最终价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText SuccessLineTemplate = FText::FromString(TEXT("……行吧行吧，{0} 金，拿去。"));

	/** 加价失败 NPC 反应（{0}=最终价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText FailureLineTemplate = FText::FromString(TEXT("嘿，手抖了吧？{0} 金，爱卖不卖。"));

	/** 「直接出手」NPC 反应（{0}=成交价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText AcceptLineTemplate = FText::FromString(TEXT("行，{0} 金，成交。"));

	/** 玩家取消时的 NPC 反应 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText CancelLine = FText::FromString(TEXT("不卖拉倒，再看会儿。"));

	// ---- NPC 演绎资产（C++ 自动播放，无需 BP 接线） ----
	// vendor 在 BeginPlay 用 NpcSkeletalMesh 给 NpcMesh 赋网格并循环播 NpcIdleAnim；
	// 讨价还价各阶段自动 PlayAnimation 对应状态动画，播完自动回 Idle。

	/** NPC 骨骼网格体（空=NpcMesh 不可见，纯 UI 讨价还价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<USkeletalMesh> NpcSkeletalMesh;

	/** 待机循环动画（平时一直播） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcIdleAnim;

	/** 报价动画（讨价还价 UI 开启） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcOfferAnim;

	/** 加价成功动画 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcSuccessAnim;

	/** 加价失败动画 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcFailureAnim;

	/** 取消动画 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcCancelAnim;

	/** 「直接出手」动画（接受参考价，不讨价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC")
	TObjectPtr<UAnimSequence> NpcAcceptAnim;
};
