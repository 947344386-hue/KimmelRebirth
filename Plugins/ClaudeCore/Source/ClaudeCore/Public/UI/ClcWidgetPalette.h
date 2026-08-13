// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClcWidgetPalette.generated.h"

/**
 * UI 配色常量集 —— 集中管理 widget 暗底/面板底色，避免各处硬编码 FLinearColor。
 *
 * 用法：RootBorder->SetBrushColor(ClcWidgetPalette::PanelDark());
 * 需要不同 alpha：ClcWidgetPalette::PanelDark().WithAlpha(0.92f);
 *
 * 颜色语义：
 *  - PanelDark：主暗底（全屏遮罩/根面板），RGB(0.02,0.03,0.05)
 *  - CardDark：卡片底（信息卡/容器），RGB(0.02,0.03,0.06)
 *  - ConfirmDark：确认区底（弹窗/确认框），RGB(0.05,0.05,0.08)
 */
USTRUCT()
struct FClcWidgetPalette
{
	GENERATED_BODY()

	/** 主暗底（全屏遮罩/根面板）—— default alpha=0.95 */
	static CLAUDECORE_API FLinearColor PanelDark(float InAlpha = 0.95f) { return FLinearColor(0.02f, 0.03f, 0.05f, InAlpha); }

	/** 卡片底（信息卡/容器）—— default alpha=0.78 */
	static CLAUDECORE_API FLinearColor CardDark(float InAlpha = 0.78f) { return FLinearColor(0.02f, 0.03f, 0.06f, InAlpha); }

	/** 确认区底（弹窗/确认框）—— default alpha=0.9 */
	static CLAUDECORE_API FLinearColor ConfirmDark(float InAlpha = 0.9f) { return FLinearColor(0.05f, 0.05f, 0.08f, InAlpha); }
};
