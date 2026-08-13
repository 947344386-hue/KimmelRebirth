// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcQuestTrackerWidget.h"
#include "UI/ClcQuestEntryWidget.h"
#include "Quest/ClcQuestSubsystem.h"
#include "Quest/ClcQuestTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

UClcQuestTrackerWidget::UClcQuestTrackerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	EntryWidgetClass = UClcQuestEntryWidget::StaticClass();
}

void UClcQuestTrackerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();
}

void UClcQuestTrackerWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DefaultRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	// 左侧竖排容器：主标题 + 主线列表 + 支线标题 + 支线列表
	UVerticalBox* OuterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OuterBox"));

	auto MakeSectionTitle = [&](const FString& Label, const FLinearColor& Color) -> UTextBlock*
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*Label));
		T->SetText(FText::FromString(Label));
		T->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = T->GetFont();
		Font.Size = 16;
		T->SetFont(Font);
		return T;
	};

	UTextBlock* MainTitle = MakeSectionTitle(TEXT("主线"), FLinearColor(1.0f, 0.85f, 0.3f));
	MainTitleText = MainTitle;
	OuterBox->AddChildToVerticalBox(MainTitle);

	MainQuestList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainQuestList"));
	OuterBox->AddChildToVerticalBox(MainQuestList);

	UTextBlock* SideTitle = MakeSectionTitle(TEXT("支线"), FLinearColor(0.7f, 0.85f, 1.0f));
	SideTitleText = SideTitle;
	if (UVerticalBoxSlot* SideTitleSlot = OuterBox->AddChildToVerticalBox(SideTitle))
	{
		SideTitleSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}

	SideQuestList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SideQuestList"));
	OuterBox->AddChildToVerticalBox(SideQuestList);

	// 置于屏幕左侧偏下
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(OuterBox);
	PanelSlot->SetAnchors(FAnchors(0.0f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.0f, 0.5f));
	PanelSlot->SetPosition(FVector2D(30.0f, -80.0f));
	PanelSlot->SetAutoSize(true);
}

void UClcQuestTrackerWidget::RefreshDisplay(UClcQuestSubsystem* Subsystem)
{
	if (!MainQuestList || !SideQuestList || !Subsystem) return;

	PopulateList(MainQuestList, EClcQuestCategory::MainQuest, Subsystem);
	PopulateList(SideQuestList, EClcQuestCategory::SideQuest, Subsystem);

	// 空列表时隐藏对应标题栏
	if (MainTitleText)
	{
		MainTitleText->SetVisibility(
			MainQuestList->GetChildrenCount() > 0
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (SideTitleText)
	{
		SideTitleText->SetVisibility(
			SideQuestList->GetChildrenCount() > 0
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void UClcQuestTrackerWidget::PopulateList(UVerticalBox* List, EClcQuestCategory Category, UClcQuestSubsystem* Subsystem)
{
	// 动态行由 CreateWidget 创建、不在 WidgetTree 里（不能用 WidgetTree->RemoveWidget，
	// 否则触发 ensure）。逐个 RemoveFromParent 保证行的 NativeDestruct 正常执行。
	for (int32 Index = List->GetChildrenCount() - 1; Index >= 0; --Index)
	{
		if (UWidget* Child = List->GetChildAt(Index))
		{
			Child->RemoveFromParent();
		}
	}
	List->ClearChildren();

	// 先收集匹配任务，按 ObjectiveType → 显示名长度排序，再构建 UI 行
	TArray<FName> SortedIDs;
	{
		const TMap<FName, FClcQuestRuntimeState>& States = Subsystem->GetRuntimeStates();
		for (const auto& Pair : States)
		{
			const FClcQuestData* Def = Subsystem->FindQuestDef(Pair.Key);
			if (!Def) continue;
			if (Def->Category != Category) continue;
			if (Pair.Value.State != EClcQuestState::Active) continue;
			if (!Def->bShowOnTracker) continue;
			SortedIDs.Add(Pair.Key);
		}
		SortedIDs.Sort([Subsystem](FName A, FName B)
		{
			const FClcQuestData* DefA = Subsystem->FindQuestDef(A);
			const FClcQuestData* DefB = Subsystem->FindQuestDef(B);
			if (!DefA || !DefB) return false;

			// 排序优先级：增量型 > 绝对次数型 > 绝对布尔型，同级按显示名长度
			auto Rank = [](EClcQuestObjectiveType T) -> int32
			{
				switch (T)
				{
				case EClcQuestObjectiveType::CutStones:
				case EClcQuestObjectiveType::UseWorkbench:
				case EClcQuestObjectiveType::BuyStones:
				case EClcQuestObjectiveType::SellStones:
				case EClcQuestObjectiveType::RepairTool:
					return 0; // 增量型
				case EClcQuestObjectiveType::EarnGold:
				case EClcQuestObjectiveType::ReachGoldTotal:
					return 1; // 绝对次数型
				case EClcQuestObjectiveType::UnlockUpgrade:
				case EClcQuestObjectiveType::ToolDamaged:
					return 2; // 绝对布尔型
				default:
					return 3;
				}
			};
			const int32 RankA = Rank(DefA->ObjectiveType);
			const int32 RankB = Rank(DefB->ObjectiveType);
			if (RankA != RankB) return RankA < RankB;
			const int32 LenA = DefA->GetDisplayName().ToString().Len();
			const int32 LenB = DefB->GetDisplayName().ToString().Len();
			return LenA < LenB;
		});
	}

	for (FName QuestID : SortedIDs)
	{
		const FClcQuestData* Def = Subsystem->FindQuestDef(QuestID);
		if (!Def) continue;

		if (!Subsystem->FindRuntimeState(QuestID)) continue;

		FClcQuestEntryView View;
		View.QuestID = QuestID;
		// 显示名：留空时由 GetDisplayName 按 ObjectiveType+ObjectiveParam 自动生成
		View.DisplayName = Def->GetDisplayName();
		// 进度文本（绝对型由 Subsystem 实时从 Backpack 读）
		View.ProgressText = Subsystem->GetQuestProgressText(QuestID);
		// 进度数值：绝对数值型实时读（进度条用），增量型=存档进度
		View.CurrentProgress = Subsystem->GetQuestProgressValue(QuestID);
		View.ObjectiveParam = Def->ObjectiveParam;
		View.ObjectiveType = Def->ObjectiveType;
		View.Category = Category;

		UClcQuestEntryWidget* Entry = CreateWidget<UClcQuestEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
		if (!Entry) continue;

		Entry->SetupEntry(View);
		if (UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Entry))
		{
			RowSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		}
	}
}
