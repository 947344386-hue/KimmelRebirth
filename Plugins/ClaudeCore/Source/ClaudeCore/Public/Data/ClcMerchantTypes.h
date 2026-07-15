// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcMerchantTypes.generated.h"

/**
 * 摊位档位——根据剩余石头综合价值判定
 */
UENUM(BlueprintType)
enum class EClcStallTier : uint8
{
	Good = 0 UMETA(DisplayName = "好摊"),
	Mid  = 1 UMETA(DisplayName = "中摊"),
	Bad  = 2 UMETA(DisplayName = "烂摊")
};

/**
 * 上次购买结果——决定气泡反馈文字
 */
UENUM(BlueprintType)
enum class EClcPurchaseOutcome : uint8
{
	None     = 0 UMETA(DisplayName = "还没买"),
	TookGood = 1 UMETA(DisplayName = "买走好的"),
	TookBad  = 2 UMETA(DisplayName = "买走烂的")
};

/**
 * 气泡文字状态池——一个 (档位 × 购买结果) 组合对应一组文字
 */
USTRUCT(BlueprintType)
struct FClcBubbleStatePool
{
	GENERATED_BODY()

	/** 摊位档位 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EClcStallTier Tier = EClcStallTier::Mid;

	/** 上次购买结果 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EClcPurchaseOutcome LastOutcome = EClcPurchaseOutcome::None;

	/** 该状态文字池（运行时随机选一句） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> Lines;
};

class UClcMerchantPersonality;

/**
 * 嘴上话术交互状态
 */
UENUM(BlueprintType)
enum class ETalkState : uint8
{
	Enter    = 0 UMETA(DisplayName = "走近"),
	Aim      = 1 UMETA(DisplayName = "瞄准"),
	Purchase = 2 UMETA(DisplayName = "购入后")
};

/**
 * 嘴上话术池——(性格 × 交互状态 × 声称档位) 对应一组文字。
 * Personality 为空 = 通用池，任意性格匹配（作 fallback）。
 */
USTRUCT(BlueprintType)
struct FClcTalkPool
{
	GENERATED_BODY()

	/** 性格（空=通用池，作 fallback） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UClcMerchantPersonality> Personality = nullptr;

	/** 交互状态 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETalkState State = ETalkState::Enter;

	/** 声称档位（商人撒谎后嘴上说的档位，不一定等于真实档位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EClcStallTier ClaimedTier = EClcStallTier::Mid;

	/** 该状态话术池（运行时随机选一句） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> Lines;
};
