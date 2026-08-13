// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcSaveSlotListWidget.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Data/ClcSessionTypes.h"
#include "UI/ClcWidgetPalette.h"
#include "ClcLog.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

// ---- 槽位行 ----

UClcSaveSlotRowWidget::UClcSaveSlotRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcSaveSlotRowWidget::InitRow(const FString& InSlotName, const FString& InTitle,
	const FString& InTime, const FString& InGold, const FString& InPlayTime,
	bool bEnabled, bool bEmpty)
{
	SlotName = InSlotName;

	auto SetSeg = [](UTextBlock* Text, const FString& Value)
	{
		if (!Text) return;
		if (Value.IsEmpty())
		{
			Text->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Text->SetText(FText::FromString(Value));
	};
	SetSeg(SlotTitleText, InTitle);
	SetSeg(SlotTimeText, InTime);
	SetSeg(SlotGoldText, InGold);
	SetSeg(SlotPlayTimeText, InPlayTime);

	// 空槽位折叠分割线
	if (DividerlineBorder)
	{
		DividerlineBorder->SetVisibility(
			bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// 纯 C++ 默认行：只有一个拼接标签，把四段拼回去
	if (!SlotTimeText && !SlotGoldText && !SlotPlayTimeText && SlotTitleText)
	{
		FString Combined = InTitle;
		if (!InTime.IsEmpty()) Combined += TEXT(" ｜ ") + InTime;
		if (!InGold.IsEmpty()) Combined += TEXT(" ｜ ") + InGold;
		if (!InPlayTime.IsEmpty()) Combined += TEXT(" ｜ ") + InPlayTime;
		SlotTitleText->SetText(FText::FromString(Combined));
	}

	if (SlotButton) SlotButton->SetIsEnabled(bEnabled);
}

void UClcSaveSlotRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 无 WBP：C++ 默认行（按钮 + 拼接标签）
	if (!SlotButton)
	{
		SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SlotButton"));
		WidgetTree->RootWidget = SlotButton;
	}
	if (!SlotTitleText && !SlotTimeText && !SlotGoldText && !SlotPlayTimeText)
	{
		SlotTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotTitleText"));
		SlotButton->SetContent(SlotTitleText);
	}
	SlotButton->OnClicked.RemoveDynamic(this, &UClcSaveSlotRowWidget::HandleClicked);
	SlotButton->OnClicked.AddDynamic(this, &UClcSaveSlotRowWidget::HandleClicked);
}

void UClcSaveSlotRowWidget::HandleClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSaveSlotRow] Clicked: %s"), *SlotName);
	OnPicked.Broadcast(SlotName);
}

// ---- 槽位列表 ----

UClcSaveSlotListWidget::UClcSaveSlotListWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UClcSaveSlotListWidget::InitSlotList(UClcSaveManagerSubsystem* InSaveManager, EClcSaveSlotListMode InMode)
{
	SaveManager = InSaveManager;
	Mode = InMode;

	// WBP 提供了 SlotListBox 容器 → 用 WBP 外壳；否则建 C++ 默认外壳。
	// 判定只查 SlotListBox：RootBorder/TitleText/CloseButton 都是可选锚点，
	// WBP 换皮时根控件不必叫 RootBorder（可用任意 Panel 当根）。
	if (!SlotListBox)
	{
		BuildShell();
	}

	if (TitleText)
	{
		const TCHAR* Title = TEXT("— 选择存档槽位 —");
		if (Mode == EClcSaveSlotListMode::Load) Title = TEXT("— 读取存档 —");
		else if (Mode == EClcSaveSlotListMode::Delete) Title = TEXT("— 删除存档 —");
		TitleText->SetText(FText::FromString(Title));
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &UClcSaveSlotListWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &UClcSaveSlotListWidget::HandleCloseClicked);
	}

	// 删除确认区：绑定按钮并默认隐藏
	if (ConfirmYesButton)
	{
		ConfirmYesButton->OnClicked.RemoveDynamic(this, &UClcSaveSlotListWidget::HandleConfirmYesClicked);
		ConfirmYesButton->OnClicked.AddDynamic(this, &UClcSaveSlotListWidget::HandleConfirmYesClicked);
	}
	if (ConfirmNoButton)
	{
		ConfirmNoButton->OnClicked.RemoveDynamic(this, &UClcSaveSlotListWidget::HandleConfirmNoClicked);
		ConfirmNoButton->OnClicked.AddDynamic(this, &UClcSaveSlotListWidget::HandleConfirmNoClicked);
	}
	if (ConfirmBorder)
	{
		ConfirmBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	PopulateRows();
}

FReply UClcSaveSlotListWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnSlotPicked.Broadcast(FString());
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UClcSaveSlotListWidget::BuildShell()
{
	if (!WidgetTree) return;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetPadding(FMargin(32.0f));
	RootBorder->SetBrushColor(FClcWidgetPalette::PanelDark());
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	RootBorder->SetContent(MenuBox);

	MenuBox->AddChildToVerticalBox(
		WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TopSpacer")))
		->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	MenuBox->AddChildToVerticalBox(TitleText);

	SlotListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SlotListBox"));
	MenuBox->AddChildToVerticalBox(SlotListBox);

	// 删除确认区（Delete 模式用）
	ConfirmBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmBorder"));
	ConfirmBorder->SetBrushColor(FClcWidgetPalette::ConfirmDark());
	ConfirmBorder->SetPadding(FMargin(12.0f));
	UVerticalBox* ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmBox"));
	ConfirmBorder->SetContent(ConfirmBox);

	ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmText"));
	ConfirmText->SetJustification(ETextJustify::Center);
	ConfirmBox->AddChildToVerticalBox(ConfirmText);

	ConfirmYesButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmYesButton"));
	UTextBlock* YesLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmYesButton_Label"));
	YesLbl->SetText(FText::FromString(TEXT("删除")));
	ConfirmYesButton->SetContent(YesLbl);
	ConfirmBox->AddChildToVerticalBox(ConfirmYesButton);

	ConfirmNoButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmNoButton"));
	UTextBlock* NoLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmNoButton_Label"));
	NoLbl->SetText(FText::FromString(TEXT("取消")));
	ConfirmNoButton->SetContent(NoLbl);
	ConfirmBox->AddChildToVerticalBox(ConfirmNoButton);

	MenuBox->AddChildToVerticalBox(ConfirmBorder);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButton_Label"));
	CloseLbl->SetText(FText::FromString(TEXT("返回")));
	CloseButton->SetContent(CloseLbl);
	MenuBox->AddChildToVerticalBox(CloseButton);

	MenuBox->AddChildToVerticalBox(
		WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BottomSpacer")))
		->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
}

void UClcSaveSlotListWidget::PopulateRows()
{
	UClcSaveManagerSubsystem* SM = SaveManager.Get();
	if (!SM || !SlotListBox)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSaveSlotList] PopulateRows —— SaveManager 或 SlotListBox 无效"));
		return;
	}

	SlotListBox->ClearChildren();

	// 行样式优先级：Class Defaults 手动指定 > 约定路径 WBP_SaveSlotRow > C++ 默认行
	TSubclassOf<UClcSaveSlotRowWidget> RowClass = RowWidgetClass;
	if (!RowClass)
	{
		RowClass = LoadClass<UClcSaveSlotRowWidget>(nullptr,
			TEXT("/Game/JadeBetting/UI/WBP_SaveSlotRow.WBP_SaveSlotRow_C"));
	}
	if (!RowClass) RowClass = UClcSaveSlotRowWidget::StaticClass();

	// 收集已有元数据：按槽位名索引
	TArray<FClcSaveMetaData> Existing = SM->GetSaveSlots();
	TMap<FString, FClcSaveMetaData> BySlot;
	for (const FClcSaveMetaData& M : Existing) BySlot.Add(M.SlotName, M);

	auto AddRow = [&](const FString& SlotName, const FString& Title,
		const FString& Time, const FString& Gold, const FString& PlayTime,
		bool bEnabled, bool bEmpty)
	{
		UClcSaveSlotRowWidget* Row = CreateWidget<UClcSaveSlotRowWidget>(this, RowClass);
		if (!Row) return;
		Row->InitRow(SlotName, Title, Time, Gold, PlayTime, bEnabled, bEmpty);
		Row->OnPicked.AddDynamic(this, &UClcSaveSlotListWidget::HandleSlotRowPicked);
		SlotListBox->AddChildToVerticalBox(Row);
	};

	// 有数据行的时间/金币/时长文本
	struct FMetaSegs { FString Time; FString Gold; FString Play; };
	auto MetaSegs = [](const FClcSaveMetaData& M) -> FMetaSegs
	{
		FMetaSegs S;
		S.Time = M.SaveTimestamp.ToString(TEXT("%m-%d %H:%M"));
		S.Gold = FString::Printf(TEXT("金币 %d"), M.Gold);
		S.Play = FString::Printf(TEXT("%.1fh"), M.PlayTimeHours);
		return S;
	};

	// AutoSave 行：Load 模式且存在时可点；Save 模式只读展示；Delete 模式存在时可点
	const bool bAutoExists = BySlot.Contains(UClcSaveManagerSubsystem::AutoSaveSlotName);
	if (Mode == EClcSaveSlotListMode::Load && bAutoExists)
	{
		const FClcSaveMetaData& M = BySlot[UClcSaveManagerSubsystem::AutoSaveSlotName];
		const FMetaSegs Segs = MetaSegs(M);
		AddRow(UClcSaveManagerSubsystem::AutoSaveSlotName,
			TEXT("自动存档"), Segs.Time, Segs.Gold, Segs.Play, true, /*bEmpty=*/false);
	}
	else if (Mode == EClcSaveSlotListMode::Delete && bAutoExists)
	{
		const FClcSaveMetaData& M = BySlot[UClcSaveManagerSubsystem::AutoSaveSlotName];
		const FMetaSegs Segs = MetaSegs(M);
		AddRow(UClcSaveManagerSubsystem::AutoSaveSlotName,
			TEXT("自动存档"), Segs.Time, Segs.Gold, Segs.Play, true, /*bEmpty=*/false);
	}
	else
	{
		AddRow(UClcSaveManagerSubsystem::AutoSaveSlotName,
			Mode == EClcSaveSlotListMode::Save ? TEXT("自动存档（自动覆盖，不可手动写入）") : TEXT("自动存档"),
			FString(), FString(), FString(), false, /*bEmpty=*/!bAutoExists);
	}

	// 手动槽位 ManualSlot_0..N-1
	for (int32 i = 0; i < SM->GetMaxSaveSlots(); ++i)
	{
		const FString SlotName = UClcSaveManagerSubsystem::MakeManualSlotName(i);
		const FClcSaveMetaData* M = BySlot.Find(SlotName);

		if (M)
		{
			const FMetaSegs Segs = MetaSegs(*M);
			AddRow(SlotName, FString::Printf(TEXT("存档 %d"), i + 1),
				Segs.Time, Segs.Gold, Segs.Play, true, /*bEmpty=*/false);
		}
		else
		{
			// Delete 模式空槽不可点；Save 模式空槽可写
			const bool bEnabled = (Mode == EClcSaveSlotListMode::Save);
			AddRow(SlotName, FString::Printf(TEXT("存档 %d（空）"), i + 1),
				FString(), FString(), FString(), bEnabled, /*bEmpty=*/true);
		}
	}

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSaveSlotList] PopulateRows done —— mode=%d, maxSlots=%d"),
		(int32)Mode, SM->GetMaxSaveSlots());
}

void UClcSaveSlotListWidget::HandleSlotRowPicked(const FString& SlotName)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSaveSlotList] Slot picked: %s (mode=%d)"), *SlotName, (int32)Mode);

	UClcSaveManagerSubsystem* SM = SaveManager.Get();

	if (Mode == EClcSaveSlotListMode::Save)
	{
		// Save 模式：写入存档 → 广播结果（打开方负责关闭+Toast）
		if (SM) SM->SaveGame(SlotName);
		OnSlotPicked.Broadcast(SlotName);
	}
	else if (Mode == EClcSaveSlotListMode::Delete)
	{
		// Delete 模式：弹确认区，不立即删
		PendingDeleteSlot = SlotName;
		if (ConfirmText)
		{
			ConfirmText->SetText(FText::FromString(FString::Printf(TEXT("确定删除存档 %s ？"), *SlotName)));
		}
		if (ConfirmBorder)
		{
			ConfirmBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	else
	{
		// Load 模式：广播，打开方负责加载
		OnSlotPicked.Broadcast(SlotName);
	}
}

void UClcSaveSlotListWidget::HandleConfirmYesClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSaveSlotList] Confirm delete: %s"), *PendingDeleteSlot);
	ExecuteDelete(PendingDeleteSlot);
}

void UClcSaveSlotListWidget::HandleConfirmNoClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSaveSlotList] Delete cancelled"));
	PendingDeleteSlot.Empty();
	if (ConfirmBorder)
	{
		ConfirmBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UClcSaveSlotListWidget::ExecuteDelete(const FString& SlotName)
{
	UClcSaveManagerSubsystem* SM = SaveManager.Get();
	if (SM)
	{
		SM->DeleteSave(SlotName);
	}
	PendingDeleteSlot.Empty();
	if (ConfirmBorder)
	{
		ConfirmBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	PopulateRows();
}

void UClcSaveSlotListWidget::HandleCloseClicked()
{
	OnSlotPicked.Broadcast(FString());
}
