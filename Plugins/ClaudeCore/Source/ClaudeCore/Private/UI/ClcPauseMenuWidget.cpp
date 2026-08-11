// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcPauseMenuWidget.h"
#include "Subsystems/ClcPauseMenuSubsystem.h"
#include "ClcLog.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/SlateTypes.h"

UClcPauseMenuWidget::UClcPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UClcPauseMenuWidget::SetOwningSubsystem(UClcPauseMenuSubsystem* InSubsystem)
{
	MenuSubsystem = InSubsystem;
}

void UClcPauseMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// 布局在 NativeOnInitialized 构建（参照 TeleportMenuWidget），此时 WidgetTree 已就绪
	BuildDefaultLayout();
}

void UClcPauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenuWidget] NativeConstruct —— RootWidget=%s, Visibility=%d"),
		WidgetTree && WidgetTree->RootWidget ? *WidgetTree->RootWidget->GetName() : TEXT("null"),
		(int32)GetVisibility());

	// 绑按钮（Remove 再 Add 防重复，参照 TeleportMenuWidget）
	if (ResumeButton)    { ResumeButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleResumeClicked); ResumeButton->OnClicked.AddDynamic(this, &UClcPauseMenuWidget::HandleResumeClicked); }
	if (SaveButton)      { SaveButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleSaveClicked); SaveButton->OnClicked.AddDynamic(this, &UClcPauseMenuWidget::HandleSaveClicked); }
	if (LoadButton)      { LoadButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleLoadClicked); LoadButton->OnClicked.AddDynamic(this, &UClcPauseMenuWidget::HandleLoadClicked); }
	if (MainMenuButton)  { MainMenuButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleMainMenuClicked); MainMenuButton->OnClicked.AddDynamic(this, &UClcPauseMenuWidget::HandleMainMenuClicked); }
	if (QuitButton)      { QuitButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleQuitClicked); QuitButton->OnClicked.AddDynamic(this, &UClcPauseMenuWidget::HandleQuitClicked); }
}

void UClcPauseMenuWidget::NativeDestruct()
{
	if (ResumeButton)    ResumeButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleResumeClicked);
	if (SaveButton)      SaveButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleSaveClicked);
	if (LoadButton)      LoadButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleLoadClicked);
	if (MainMenuButton)  MainMenuButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleMainMenuClicked);
	if (QuitButton)      QuitButton->OnClicked.RemoveDynamic(this, &UClcPauseMenuWidget::HandleQuitClicked);

	Super::NativeDestruct();
}

FReply UClcPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (MenuSubsystem.IsValid())
		{
			MenuSubsystem->ResumeGame();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---- 按钮回调 ----

void UClcPauseMenuWidget::HandleResumeClicked()
{
	if (MenuSubsystem.IsValid()) MenuSubsystem->ResumeGame();
}

void UClcPauseMenuWidget::HandleSaveClicked()
{
	if (MenuSubsystem.IsValid()) MenuSubsystem->ManualSave();
}

void UClcPauseMenuWidget::HandleLoadClicked()
{
	if (MenuSubsystem.IsValid()) MenuSubsystem->LoadSave();
}

void UClcPauseMenuWidget::HandleMainMenuClicked()
{
	if (MenuSubsystem.IsValid()) MenuSubsystem->GoToMainMenu();
}

void UClcPauseMenuWidget::HandleQuitClicked()
{
	if (MenuSubsystem.IsValid()) MenuSubsystem->QuitGame();
}

// ---- 默认布局（参照 UClcTeleportMenuWidget：UBorder 做根，SetContent 放内容） ----

void UClcPauseMenuWidget::BuildDefaultLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcPauseMenuWidget] BuildDefaultLayout skipped —— WidgetTree=%s, RootWidget=%s"),
			WidgetTree ? TEXT("ok") : TEXT("null"),
			WidgetTree && WidgetTree->RootWidget ? *WidgetTree->RootWidget->GetName() : TEXT("null"));
		return;
	}

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenuWidget] BuildDefaultLayout"));

	// 根：UBorder 全屏半透明暗色背景（参照 TeleportMenuWidget）
	UBorder* RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RootBorder"));
	RootBorder->SetPadding(FMargin(32.0f));
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
	WidgetTree->RootWidget = RootBorder;

	// 内容：居中 VerticalBox
	UVerticalBox* MenuBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	RootBorder->SetContent(MenuBox);

	// 垂直居中留白
	MenuBox->AddChildToVerticalBox(
		WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TopSpacer")))
		->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);

	auto MakeBtn = [&](const TCHAR* Name, const FString& Label) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(FString(Name) + TEXT("_Label")));
		LabelWidget->SetText(FText::FromString(Label));
		Btn->SetContent(LabelWidget);
		MenuBox->AddChildToVerticalBox(Btn);
		return Btn;
	};

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("— 暂停 —")));
	TitleText->SetJustification(ETextJustify::Center);
	MenuBox->AddChildToVerticalBox(TitleText);

	ResumeButton   = MakeBtn(TEXT("ResumeButton"),   TEXT("继续游戏"));
	SaveButton     = MakeBtn(TEXT("SaveButton"),     TEXT("手动存档"));
	LoadButton     = MakeBtn(TEXT("LoadButton"),     TEXT("加载存档"));
	MainMenuButton = MakeBtn(TEXT("MainMenuButton"), TEXT("回到主界面"));
	QuitButton     = MakeBtn(TEXT("QuitButton"),     TEXT("退出游戏"));

	// 底部留白撑满
	MenuBox->AddChildToVerticalBox(
		WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BottomSpacer")))
		->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenuWidget] BuildDefaultLayout done —— children=%d"), MenuBox->GetChildrenCount());
}