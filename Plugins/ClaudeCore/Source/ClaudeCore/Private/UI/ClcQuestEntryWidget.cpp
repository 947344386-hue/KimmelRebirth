// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcQuestEntryWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/ProgressBar.h"
#include "Styling/SlateColor.h"

void UClcQuestEntryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcQuestEntryWidget::SetupEntry(const FClcQuestEntryView& View)
{
	CachedQuestID = View.QuestID;
	CachedDisplayName = View.DisplayName;
	CachedProgressText = View.ProgressText;
	CachedCurrentProgress = View.CurrentProgress;
	CachedObjectiveParam = View.ObjectiveParam;
	CachedObjectiveType = View.ObjectiveType;
	CachedCategory = View.Category;

	if (DescText)
	{
		DescText->SetText(View.DisplayName);
	}
	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(View.ProgressText));
	}
	if (CompleteCheck)
	{
		// GetQuestProgressText 对布尔型返回 "✓"/"✗"；当前面板只显示 Active 任务，
		// 故可见行的勾选恒为未勾——按数据正确设置，为将来显示 Completed 行预留。
		CompleteCheck->SetIsChecked(View.ProgressText == TEXT("✓"));
	}
	if (ProgressBar)
	{
		const float Ratio = FMath::Clamp(
			(View.ObjectiveParam > 0) ? (float)View.CurrentProgress / (float)View.ObjectiveParam : 0.0f,
			0.0f, 1.0f);
		ProgressBar->SetPercent(Ratio);
	}

	ApplyWidgetVisibility();
}

void UClcQuestEntryWidget::ApplyWidgetVisibility()
{
	// 绝对布尔型：进度文本无意义，勾选框指示达成；其余类型进度文本显示、勾选框隐藏
	const bool bBoolean =
		CachedObjectiveType == EClcQuestObjectiveType::UnlockUpgrade ||
		CachedObjectiveType == EClcQuestObjectiveType::ToolDamaged;

	if (ProgressText)
	{
		ProgressText->SetVisibility(bBoolean ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (CompleteCheck)
	{
		CompleteCheck->SetVisibility(bBoolean ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ProgressBar)
	{
		// 进度条仅主线 + 绝对数值型目标显示
		const bool bAbsoluteNumeric =
			CachedObjectiveType == EClcQuestObjectiveType::EarnGold ||
			CachedObjectiveType == EClcQuestObjectiveType::ReachGoldTotal;
		const bool bShowBar = bAbsoluteNumeric && CachedCategory == EClcQuestCategory::MainQuest;
		ProgressBar->SetVisibility(bShowBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UClcQuestEntryWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DefaultRowRoot"));
	WidgetTree->RootWidget = Row;

	DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescText"));
	DescText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f)));
	{
		FSlateFontInfo Font = DescText->GetFont();
		Font.Size = 13;
		DescText->SetFont(Font);
	}
	if (UHorizontalBoxSlot* NSlot = Row->AddChildToHorizontalBox(DescText))
	{
		NSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	}

	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ProgressText"));
	ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	{
		FSlateFontInfo Font = ProgressText->GetFont();
		Font.Size = 13;
		ProgressText->SetFont(Font);
	}
	Row->AddChildToHorizontalBox(ProgressText);

	CompleteCheck = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("CompleteCheck"));
	Row->AddChildToHorizontalBox(CompleteCheck);

	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressBar"));
	ProgressBar->SetFillColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.4f));
	if (UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(ProgressBar))
	{
		// 给固定宽度，否则进度条 0×0 不渲染
		BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}
