// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcBackpackWidget.h"

void UClcBackpackWidget::SelectStone(int32 StoneIndex)
{
	OnStoneSelected.Broadcast(StoneIndex);
}
