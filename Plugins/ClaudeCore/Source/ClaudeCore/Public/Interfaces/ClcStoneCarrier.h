// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneCarrier.generated.h"

UINTERFACE()
class CLAUDECORE_API UClcStoneCarrier : public UInterface
{
	GENERATED_BODY()
};

class CLAUDECORE_API IClcStoneCarrier
{
	GENERATED_BODY()
public:
	virtual TArray<FClcStoneRuntimeData> GetStones() const { return TArray<FClcStoneRuntimeData>(); }
	virtual int32 AddStone(const FClcStoneRuntimeData& StoneData) { return -1; }
	virtual bool RemoveStone(int32 StoneIndex) { return false; }
	virtual int32 GetGold() const { return 0; }
	virtual void AddGold(int32 Amount) {}
	virtual bool SpendGold(int32 Amount) { return false; }
};