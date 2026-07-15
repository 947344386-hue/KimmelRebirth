// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcMerchantTalkConfig.h"
#include "Data/ClcMerchantPersonality.h"

UClcMerchantTalkConfig::UClcMerchantTalkConfig()
{
	// 默认通用话术池（Personality=null = 任意性格匹配，作 fallback）
	// 新建 DA_MerchantTalkConfig 时自动带这 9 条（3 状态 × 3 声称档位），体感不够再在编辑器加/改
	TalkPools.SetNum(9);

	auto Set = [this](int32 Idx, ETalkState State, EClcStallTier Tier,
		const TCHAR* L0, const TCHAR* L1)
	{
		FClcTalkPool& P = TalkPools[Idx];
		P.Personality = nullptr;
		P.State = State;
		P.ClaimedTier = Tier;
		P.Lines.Reset(2);
		P.Lines.Add(FText::FromString(L0));
		P.Lines.Add(FText::FromString(L1));
	};

	// Enter（走近整摊推销）
	Set(0, ETalkState::Enter,    EClcStallTier::Good, TEXT("随便看，都是好货"), TEXT("这块绝了"));
	Set(1, ETalkState::Enter,    EClcStallTier::Mid,  TEXT("还行还行"),        TEXT("你挑挑"));
	Set(2, ETalkState::Enter,    EClcStallTier::Bad,  TEXT("便宜处理了"),      TEXT("走过别错过"));
	// Aim（瞄准单块评价）
	Set(3, ETalkState::Aim,      EClcStallTier::Good, TEXT("你眼光毒"),        TEXT("这块值"));
	Set(4, ETalkState::Aim,      EClcStallTier::Mid,  TEXT("这块稳"),          TEXT("能玩"));
	Set(5, ETalkState::Aim,      EClcStallTier::Bad,  TEXT("这块实惠"),        TEXT("拿去练手"));
	// Purchase（购入后反应）
	Set(6, ETalkState::Purchase, EClcStallTier::Good, TEXT("哎便宜卖你了"),    TEXT("有的赚"));
	Set(7, ETalkState::Purchase, EClcStallTier::Mid,  TEXT("成交"),            TEXT("不亏"));
	Set(8, ETalkState::Purchase, EClcStallTier::Bad,  TEXT("走一个是一个"),    TEXT("嘿成交"));
}

FText UClcMerchantTalkConfig::PickLine(UClcMerchantPersonality* Personality, ETalkState State, EClcStallTier ClaimedTier) const
{
	// 同状态同档位下：优先性格精确匹配，其次 Personality=空的通用池（fallback）
	// 档位和状态始终精确，不会跨档位选错话术
	const FClcTalkPool* ExactMatch = nullptr;
	const FClcTalkPool* WildcardMatch = nullptr;

	for (const FClcTalkPool& Pool : TalkPools)
	{
		if (Pool.State != State || Pool.ClaimedTier != ClaimedTier) continue;

		if (Pool.Personality == Personality)
		{
			ExactMatch = &Pool;
			break; // 精确匹配最优
		}
		if (!Pool.Personality && !WildcardMatch)
		{
			WildcardMatch = &Pool; // 通用池作 fallback
		}
	}

	const FClcTalkPool* Chosen = ExactMatch ? ExactMatch : WildcardMatch;
	if (Chosen && Chosen->Lines.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, Chosen->Lines.Num() - 1);
		return Chosen->Lines[Idx];
	}
	return FText::GetEmpty();
}
