// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcMainMenuWidget.h"
#include "Subsystems/ClcMainMenuSubsystem.h"
#include "Data/ClcSessionTypes.h"
#include "ClcLog.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Blueprint/WidgetTree.h"

UClcMainMenuWidget::UClcMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UClcMainMenuWidget::InitializeMenu(UClcMainMenuSubsystem* InSubsystem)
{
	MenuSubsystem = InSubsystem;

	// 从 Subsystem 读取默认配置填充 UI
	FClcSessionConfig Defaults;
	if (InSubsystem)
	{
		Defaults = InSubsystem->GetDefaultSessionConfig();
	}

	// 金滑条
	if (GoldSlider)
	{
		GoldSlider->SetMinValue(MinGold);
		GoldSlider->SetMaxValue(MaxGold);
		GoldSlider->SetValue(static_cast<float>(Defaults.StartingGold));
	}

	// 难度下拉框
	if (DifficultyComboBox)
	{
		const UEnum* E = StaticEnum<EClcDifficultyPreset>();
		TArray<FString> Options;
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			Options.Add(E->GetDisplayNameTextByIndex(i).ToString());
		}
		DifficultyComboBox->ClearOptions();
		for (const FString& Opt : Options)
		{
			DifficultyComboBox->AddOption(Opt);
		}
		DifficultyComboBox->SetSelectedIndex(static_cast<int32>(Defaults.Difficulty));
	}

	UpdateGoldText(Defaults.StartingGold);
	RefreshSaveSlots();
}

void UClcMainMenuWidget::RefreshSaveSlots()
{
	if (MenuSubsystem.IsValid())
	{
		TArray<FClcSaveMetaData> Slots = MenuSubsystem->GetSaveSlots();
		const bool bHasSaves = Slots.Num() > 0;

		// 有存档 → 隐藏"无存档"提示，启用继续按钮
		if (NoSavesText)
		{
			NoSavesText->SetVisibility(bHasSaves ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		}
		if (ContinueButton)
		{
			ContinueButton->SetIsEnabled(bHasSaves);
		}
	}
}

void UClcMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UClcMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 无 WBP → 创建 C++ 默认布局
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	// 绑按钮
	if (StartButton)  StartButton->OnClicked.AddDynamic(this, &UClcMainMenuWidget::HandleStartClicked);
	if (ContinueButton) ContinueButton->OnClicked.AddDynamic(this, &UClcMainMenuWidget::HandleContinueClicked);
	if (QuitButton)   QuitButton->OnClicked.AddDynamic(this, &UClcMainMenuWidget::HandleQuitClicked);
	if (DeleteSaveButton) DeleteSaveButton->OnClicked.AddDynamic(this, &UClcMainMenuWidget::HandleDeleteSaveClicked);
	if (GoldSlider)   GoldSlider->OnValueChanged.AddDynamic(this, &UClcMainMenuWidget::HandleGoldSliderChanged);
	if (DifficultyComboBox) DifficultyComboBox->OnSelectionChanged.AddDynamic(this, &UClcMainMenuWidget::HandleDifficultyChanged);
}

void UClcMainMenuWidget::NativeDestruct()
{
	if (StartButton)  StartButton->OnClicked.RemoveDynamic(this, &UClcMainMenuWidget::HandleStartClicked);
	if (ContinueButton) ContinueButton->OnClicked.RemoveDynamic(this, &UClcMainMenuWidget::HandleContinueClicked);
	if (QuitButton)   QuitButton->OnClicked.RemoveDynamic(this, &UClcMainMenuWidget::HandleQuitClicked);
	if (DeleteSaveButton) DeleteSaveButton->OnClicked.RemoveDynamic(this, &UClcMainMenuWidget::HandleDeleteSaveClicked);
	if (GoldSlider)   GoldSlider->OnValueChanged.RemoveDynamic(this, &UClcMainMenuWidget::HandleGoldSliderChanged);
	if (DifficultyComboBox) DifficultyComboBox->OnSelectionChanged.RemoveDynamic(this, &UClcMainMenuWidget::HandleDifficultyChanged);

	Super::NativeDestruct();
}

FReply UClcMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleQuitClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---- 按钮 ----

void UClcMainMenuWidget::HandleStartClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenuWidget] 开始新游戏"));
	if (MenuSubsystem.IsValid())
	{
		MenuSubsystem->StartNewGame(BuildSessionConfig());
	}
}

void UClcMainMenuWidget::HandleContinueClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenuWidget] 继续游戏"));
	if (MenuSubsystem.IsValid())
	{
		MenuSubsystem->ContinueGame(TEXT("AutoSave"));
	}
}

void UClcMainMenuWidget::HandleQuitClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenuWidget] 退出游戏"));
	if (MenuSubsystem.IsValid())
	{
		MenuSubsystem->QuitGame();
	}
}

void UClcMainMenuWidget::HandleDeleteSaveClicked()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenuWidget] 删除存档"));
}

void UClcMainMenuWidget::HandleGoldSliderChanged(float Value)
{
	UpdateGoldText(FMath::Clamp(FMath::RoundToInt(Value), MinGold, MaxGold));
}

void UClcMainMenuWidget::HandleDifficultyChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcMainMenuWidget] 难度变更: %s"), *SelectedItem);
}

// ---- 内部 ----

FClcSessionConfig UClcMainMenuWidget::BuildSessionConfig() const
{
	FClcSessionConfig Config;
	Config.StartingGold = GoldSlider ? FMath::Clamp(FMath::RoundToInt(GoldSlider->GetValue()), MinGold, MaxGold) : DefaultGold;

	if (DifficultyComboBox)
	{
		const UEnum* E = StaticEnum<EClcDifficultyPreset>();
		const int32 Idx = E->GetIndexByNameString(DifficultyComboBox->GetSelectedOption());
		Config.Difficulty = (Idx != INDEX_NONE) ? static_cast<EClcDifficultyPreset>(E->GetValueByIndex(Idx)) : EClcDifficultyPreset::Normal;
		if (Config.Difficulty != EClcDifficultyPreset::Custom)
		{
			Config.DifficultyMultiplier = ClcDifficultyPenaltyMultiplier(Config.Difficulty);
		}
	}
	else
	{
		Config.Difficulty = EClcDifficultyPreset::Normal;
		Config.DifficultyMultiplier = 1.0f;
	}
	Config.bIsNewGame = true;
	return Config;
}

void UClcMainMenuWidget::UpdateGoldText(int32 Gold)
{
	if (GoldValueText)
	{
		GoldValueText->SetText(FText::FromString(FString::Printf(TEXT("起始金币：%d"), Gold)));
	}
}

void UClcMainMenuWidget::BuildDefaultLayout()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenuWidget] BuildDefaultLayout — 创建 C++ 默认布局"));
	if (!WidgetTree) return;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootBox"));
	if (!Root) return;
	WidgetTree->RootWidget = Root;

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (TitleText) { TitleText->SetText(FText::FromString(TEXT("赌石传说"))); Root->AddChildToVerticalBox(TitleText); }

	GoldValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GoldValueText"));
	if (GoldValueText) { GoldValueText->SetText(FText::FromString(TEXT("起始金币：50000"))); Root->AddChildToVerticalBox(GoldValueText); }

	GoldSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("GoldSlider"));
	if (GoldSlider) { GoldSlider->SetMinValue(MinGold); GoldSlider->SetMaxValue(MaxGold); GoldSlider->SetValue(DefaultGold); Root->AddChildToVerticalBox(GoldSlider); }

	DifficultyComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("DifficultyComboBox"));
	if (DifficultyComboBox) Root->AddChildToVerticalBox(DifficultyComboBox);

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	if (StartButton) Root->AddChildToVerticalBox(StartButton);

	ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
	if (ContinueButton) Root->AddChildToVerticalBox(ContinueButton);

	QuitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuitButton"));
	if (QuitButton) Root->AddChildToVerticalBox(QuitButton);
}
