// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcMerchantBubbleConfig.h"

FText UClcMerchantBubbleConfig::PickLine(EClcStallTier Tier, EClcPurchaseOutcome Outcome) const
{
	for (const FClcBubbleStatePool& Pool : BubblePools)
	{
		if (Pool.Tier == Tier && Pool.LastOutcome == Outcome)
		{
			if (Pool.Lines.Num() <= 0) break;
			const int32 Idx = FMath::RandRange(0, Pool.Lines.Num() - 1);
			return Pool.Lines[Idx];
		}
	}
	return FText::GetEmpty();
}
