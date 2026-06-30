// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcEditorSubsystem.h"
#include "ToolMenus.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"

void UClcEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		RegisterToolbarButton();
	}));
}

void UClcEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UClcEditorSubsystem::RegisterToolbarButton()
{
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!Toolbar) return;

	FToolMenuSection& Section = Toolbar->FindOrAddSection("ClaudeCore");

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		"ToggleLanguage",
		FUIAction(
			FExecuteAction::CreateStatic(&UClcEditorSubsystem::OnToggleLanguage)
		),
		FText::FromString(TEXT("中/EN")),
		FText::FromString(TEXT("切换编辑器语言（中文/English）"))
	));
}

void UClcEditorSubsystem::OnToggleLanguage()
{
	FCultureRef CurrentCulture = FInternationalization::Get().GetCurrentCulture();
	FString Lang = CurrentCulture->GetTwoLetterISOLanguageName();

	if (Lang.Equals(TEXT("zh"), ESearchCase::IgnoreCase))
	{
		FInternationalization::Get().SetCurrentCulture(TEXT("en"));
	}
	else
	{
		FInternationalization::Get().SetCurrentCulture(TEXT("zh-Hans"));
	}
}
