// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcStoneEntryWidget.h"
#include "UI/ClcStoneTooltipWidget.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

void UClcStoneEntryWidget::InitializeEntry(int32 Index, const FClcStoneRuntimeData& Data)
{
	StoneIndex = Index;
	StoneData = Data;

	// 自动刷新名称 TextBlock（BP 里有 NameText 组件才生效）
	if (NameText)
	{
		NameText->SetText(FText::FromString(Data.DisplayName));
	}
}

void UClcStoneEntryWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InPointerEvent);

	// 没配 TooltipClass 或没初始化数据 → 不显示
	if (!TooltipClass || StoneIndex < 0)
	{
		return;
	}

	// 先清掉残留（保险——理论上 Leave 已经清了）
	DestroyTooltip();

	UClcStoneMarketSubsystem* Market = GetMarketSubsystem();
	if (!Market)
	{
		return;
	}

	// C++ 端组装数据 + 创建 + 定位 + 显示
	const FClcStoneTooltipInfo Info = Market->BuildTooltipInfo(StoneData);
	TooltipRef = UClcStoneTooltipWidget::ShowTooltipNextTo(
		this,          // WorldContext
		this,          // AnchorWidget（StoneEntry 自己当锚）
		Info,
		TooltipClass);
}

void UClcStoneEntryWidget::NativeOnMouseLeave(const FPointerEvent& InPointerEvent)
{
	Super::NativeOnMouseLeave(InPointerEvent);
	DestroyTooltip();
}

void UClcStoneEntryWidget::RemoveFromParent()
{
	// 先调 Super（真正执行移除），再清 tooltip
	Super::RemoveFromParent();
	DestroyTooltip();
}

void UClcStoneEntryWidget::NativeDestruct()
{
	// widget 被 GC/销毁前的最后兜底——前面 RemoveFromParent 递归清理万一漏了
	// （比如工作台走了非标准关闭路径），这里保证 tooltip 一定被清掉
	DestroyTooltip();
	Super::NativeDestruct();
}

void UClcStoneEntryWidget::DestroyTooltip()
{
	if (TooltipRef)
	{
		TooltipRef->Hide();
		TooltipRef = nullptr;
	}
}

UClcStoneMarketSubsystem* UClcStoneEntryWidget::GetMarketSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>() : nullptr;
}
