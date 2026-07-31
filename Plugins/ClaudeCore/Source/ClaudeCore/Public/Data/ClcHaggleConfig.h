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

	// ---- 高价值事件 NPC 台词（{0}=石头名 {1}=价格） ----

	/** 进入出售模式 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcEnterModeLine = FText::FromString(TEXT("来了？今儿有好货，上台看看。"));

	/** 退出出售模式（主动离开） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcExitModeLine = FText::FromString(TEXT("下次再来啊。"));

	/** 石头上台——涨了（{0}=石头名 {1}=当前回收价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneProfitLine = FText::FromString(TEXT("「{0}」……这块赚了啊，现在值 {1} 金。"));

	/** 石头上台——跌了（{0}=石头名 {1}=当前回收价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneLossLine = FText::FromString(TEXT("「{0}」……啧，现在只值 {1} 金，不太好看。"));

	/** 石头上台——持平（{0}=石头名 {1}=当前回收价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneEvenLine = FText::FromString(TEXT("「{0}」，目前值 {1} 金，不赚不赔。"));

	/** 石头上台——从未开过窗（{0}=石头名） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneUnopenedLine = FText::FromString(TEXT("「{0}」——没动过刀的新石头？这可有点赌。"));

	/** 石头上台——开过窗纯杂未见玉（{0}=石头名） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneNoJadeLine = FText::FromString(TEXT("「{0}」……开了一窗全是杂，不好办哪。"));

	/** 石头上台——已见玉（{0}=石头名） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneJadeLine = FText::FromString(TEXT("「{0}」……见绿了，种头还行。"));

	/** 石头上台失败 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStonePlaceFailedLine = FText::FromString(TEXT("嗯？这石头拿不起来，换一块。"));

	/** 售出完成——涨了（{0}=石头名 {1}=成交价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcSoldProfitLine = FText::FromString(TEXT("「{0}」{1} 金——你赚了，下回可没这好事！"));

	/** 售出完成——跌了（{0}=石头名 {1}=成交价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcSoldLossLine = FText::FromString(TEXT("「{0}」{1} 金——亏了吧？下次擦亮眼。"));

	/** 售出完成——持平（{0}=石头名 {1}=成交价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcSoldEvenLine = FText::FromString(TEXT("「{0}」{1} 金，不亏不赚。"));

	/** 售出完成——背包空（{0}=石头名 {1}=成交价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcSoldAllLine = FText::FromString(TEXT("卖空了？慢走，下次带好货来。"));

	/** 锁价石上台——不能用 QTE（{0}=石头名 {1}=锁定价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|Dialogue")
	FText NpcStoneLockedLine = FText::FromString(TEXT("「{0}」已锁价 {1} 金，直接出手就成，不能再谈。"));

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

	// ---- 高价值事件动画（C++ 自动播放，空=跳过播放，不报错） ----

	/** 玩家进入回收台范围 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcPlayerEnterAnim;

	/** 玩家离开回收台范围 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcPlayerLeaveAnim;

	/** 石头上台（通用——涨/跌/未开由调用方选择具体动画） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStonePlacedAnim;

	/** 石头上台——涨了（回收价 > 购入价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStoneProfitAnim;

	/** 石头上台——跌了（回收价 < 购入价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStoneLossAnim;

	/** 石头上台——从未开过窗 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStoneUnopenedAnim;

	/** 石头上台——开过窗但没开到玉（纯杂） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStoneNoJadeAnim;

	/** 石头上台——已开到玉（种水暴露） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStoneJadeAnim;

	/** 石头上台失败（Spawn/Init 失败，纯异常） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcStonePlacedFailedAnim;

	/** 售出完成 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcSoldAnim;

	/** 售出后背包空（NPC 送客/告别） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcSoldAllAnim;

	/** 进入出售模式 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcEnterModeAnim;

	/** 退出出售模式（主动或售空） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Haggle|NPC|HighValue")
	TObjectPtr<UAnimSequence> NpcExitModeAnim;
};
