// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimSequence.h"
#include "ClcMerchantAnimConfig.generated.h"

/**
 * 商人动画池配置——按状态分池，每池是动画引用数组，体感不对在编辑器拖引用调整。
 *
 * 整摊情绪：全档位用 ConfidentMoodPool（商人不会主动释放负面信号，烂摊也演成好摊）。
 * NervousMoodPool 默认留空（最高欺骗）；填入后烂摊才用，作为可选低压泄漏信号。
 *
 * 单块微反应：诚实泄漏——Greedy（好石头）/ Eager（烂石头）/ Neutral（中石头）。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantAnimConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- 按状态取动画（池空返回 nullptr，调用方自行兜底）----

	/** 基础待机（无玩家附近，单条循环） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickIdle() const { return IdleAnim; }

	/** 整摊自信推销（全档位共用） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickConfidentMood() const;

	/** 整摊心虚泄漏（烂摊可选；池空返回 nullptr，调用方回退到 Confident） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickNervousMood() const;

	/** 单块微反应 - 贪/不舍（好石头） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickGreedyReaction() const;

	/** 单块微反应 - 急切脱手（烂石头） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickEagerReaction() const;

	/** 单块微反应 - 中性（中石头） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantAnim")
	UAnimSequence* PickNeutralReaction() const;

protected:
	/** 基础待机（无玩家附近，单条循环） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	UAnimSequence* IdleAnim = nullptr;

	// ---- 整摊情绪 ----

	/** 自信推销池（全档位共用，商人总演成好摊） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mood|Confident", meta = (TitleTooltip = "全档位共用；商人不会主动释放负面信号"))
	TArray<UAnimSequence*> ConfidentMoodPool;

	/** 心虚泄漏池（可选，默认留空；填入后烂摊才用） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mood|Nervous", meta = (TitleTooltip = "默认留空=最高欺骗；想加烂摊泄漏时填入"))
	TArray<UAnimSequence*> NervousMoodPool;

	// ---- 单块微反应 ----

	/** 贪/不舍池（好石头——商人知道值钱，下意识犹豫/护货） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Greedy")
	TArray<UAnimSequence*> GreedyReactionPool;

	/** 急切脱手池（烂石头——商人想甩掉，下意识兴奋招揽） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Eager")
	TArray<UAnimSequence*> EagerReactionPool;

	/** 中性池（中石头——无明显倾向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction|Neutral")
	TArray<UAnimSequence*> NeutralReactionPool;

private:
	/** 从池里随机抽一条（池空返回 nullptr） */
	static UAnimSequence* PickRandomFrom(const TArray<UAnimSequence*>& Pool);
};
