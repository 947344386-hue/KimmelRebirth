// Copyright ClaudeCore. All Rights Reserved.
#include "Subsystems/ClcEditorSubsystem.h"
#include "ToolMenus.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
void UClcEditorSubsystem::Initialize(FSubsystemCollectionBase& C) { Super::Initialize(C); UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this](){RegisterToolbarButton();})); }
void UClcEditorSubsystem::Deinitialize() { Super::Deinitialize(); }
void UClcEditorSubsystem::RegisterToolbarButton() {
	UToolMenu* T = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!T) return;
	T->FindOrAddSection("ClaudeCore").AddEntry(FToolMenuEntry::InitToolBarButton("ToggleLanguage", FUIAction(FExecuteAction::CreateStatic(&UClcEditorSubsystem::OnToggleLanguage)), FText::FromString(TEXT("中/EN")), FText::FromString(TEXT("切换编辑器语言"))));
}
void UClcEditorSubsystem::OnToggleLanguage() {
	FString L = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName();
	FInternationalization::Get().SetCurrentCulture(L.Equals(TEXT("zh"),ESearchCase::IgnoreCase) ? TEXT("en") : TEXT("zh-Hans"));
}