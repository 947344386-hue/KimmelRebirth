// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcBackpackWidget.h"
#include "UI/ClcStoneEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"

void UClcBackpackWidget::SelectStone(int32 StoneIndex)
{
	OnStoneSelected.Broadcast(StoneIndex);
}

// 递归遍历 widget + 所有 Panel 的子 widget（包括动态 Add Child 创建的）
static void ClearTooltipsRecursive(UWidget* Widget)
{
	if (!Widget) return;

	// 是 StoneEntry → 清理 tooltip
	if (UClcStoneEntryWidget* Entry = Cast<UClcStoneEntryWidget>(Widget))
	{
		Entry->ClearTooltip();
	}

	// 是 Panel → 递归遍历所有子 widget（包括动态添加的，这些不在 WidgetTree 里）
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			ClearTooltipsRecursive(Panel->GetChildAt(i));
		}
	}
}

void UClcBackpackWidget::RemoveFromParent()
{
	// 关闭前递归清理所有 StoneEntry 的 tooltip——动态 CreateWidget+AddChild 创建的
	// StoneEntry 不在 WidgetTree 里，ForEachWidget 找不到它们，必须递归遍历 Panel 子 widget
	if (WidgetTree && WidgetTree->RootWidget)
	{
		ClearTooltipsRecursive(WidgetTree->RootWidget);
	}

	Super::RemoveFromParent();
}
