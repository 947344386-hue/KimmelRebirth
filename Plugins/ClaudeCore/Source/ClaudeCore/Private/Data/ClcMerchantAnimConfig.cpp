// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcMerchantAnimConfig.h"

UAnimSequence* UClcMerchantAnimConfig::PickRandomFrom(const TArray<UAnimSequence*>& Pool)
{
	if (Pool.Num() <= 0) return nullptr;
	const int32 Idx = FMath::RandRange(0, Pool.Num() - 1);
	return Pool[Idx];
}

UAnimSequence* UClcMerchantAnimConfig::PickConfidentMood() const
{
	return PickRandomFrom(ConfidentMoodPool);
}

UAnimSequence* UClcMerchantAnimConfig::PickNervousMood() const
{
	return PickRandomFrom(NervousMoodPool);
}

UAnimSequence* UClcMerchantAnimConfig::PickGreedyReaction() const
{
	return PickRandomFrom(GreedyReactionPool);
}

UAnimSequence* UClcMerchantAnimConfig::PickEagerReaction() const
{
	return PickRandomFrom(EagerReactionPool);
}

UAnimSequence* UClcMerchantAnimConfig::PickNeutralReaction() const
{
	return PickRandomFrom(NeutralReactionPool);
}
