// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcLoadingScreenWidget.h"
#include "ClcDeveloperSettings.h"
#include "ClcLog.h"
#include "MoviePlayer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Styling/StyleDefaults.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"

// 内置默认提示池（LoadingTips 为空时用）
static const TArray<FString>& GetDefaultTips()
{
	static const TArray<FString> Tips = {
		TEXT("提示：擦石可以观察原石内部的成色，再决定是否购买。"),
		TEXT("提示：解石前先擦出窗口，能大幅降低误判风险。"),
		TEXT("提示：商人在不同时段出现，留意他给的报价档位。"),
		TEXT("提示：鹰眼技能能短暂透视，冷却较长，留到关键时刻用。"),
		TEXT("提示：讨价还价时连续命中可上浮报价，错过则下浮。"),
		TEXT("提示：自动存档每 5 分钟触发一次，也可手动存档。"),
		TEXT("提示：摊位石头价格有封顶，货比三家再下手。"),
	};
	return Tips;
}

SClcLoadingScreenWidget::SClcLoadingScreenWidget()
{
}

SClcLoadingScreenWidget::~SClcLoadingScreenWidget()
{
}

void SClcLoadingScreenWidget::Construct(const FArguments& InArgs)
{
	// 读配置
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (DS)
	{
		Backgrounds = DS->LoadingBackgrounds;
		SwitchInterval = DS->LoadingBackgroundSwitchInterval;
	}
	if (SwitchInterval < 0.5f)
	{
		SwitchInterval = 0.5f;
	}

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(BackgroundImage, SImage)
			.ColorAndOpacity(FLinearColor(0.02f, 0.03f, 0.05f, 1.0f))
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FStyleDefaults::GetNoBrush())
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
			.Padding(FMargin(0.0f))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("加载中…")))
					.ColorAndOpacity(FLinearColor(0.9f, 0.88f, 0.78f, 1.0f))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
				]
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Center)
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 48.0f))
				[
					SAssignNew(TipText, STextBlock)
					.Text(FText::FromString(PickRandomTip()))
					.ColorAndOpacity(FLinearColor(0.7f, 0.72f, 0.65f, 1.0f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.AutoWrapText(true)
					.WrapTextAt(900.0f)
				]
			]
		]
	];

	// 起始背景（无图库时 BackgroundImage 保持暗底）
	InitFirstBackground();
}

void SClcLoadingScreenWidget::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, DeltaTime);

	if (Backgrounds.Num() <= 1)
	{
		return;
	}

	TimeSinceSwitch += DeltaTime;
	if (TimeSinceSwitch >= SwitchInterval)
	{
		TimeSinceSwitch = 0.0f;
		AdvanceBackground();
	}
}

void SClcLoadingScreenWidget::InitFirstBackground()
{
	if (Backgrounds.Num() == 0)
	{
		return;
	}
	CurrentBgIndex = FMath::RandRange(0, Backgrounds.Num() - 1);
	LoadBackgroundAt(CurrentBgIndex);
}

void SClcLoadingScreenWidget::LoadBackgroundAt(int32 Index)
{
	if (!Backgrounds.IsValidIndex(Index) || !BackgroundImage.IsValid())
	{
		return;
	}

	UTexture2D* Tex = Cast<UTexture2D>(Backgrounds[Index].TryLoad());
	if (!Tex)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[LoadingScreen] 背景图加载失败 idx=%d path=%s"),
			Index, *Backgrounds[Index].ToString());
		return;
	}

	CurrentBrush = MakeShareable(new FSlateDynamicImageBrush(
		Tex, FVector2D(1920.0f, 1080.0f), FName(*Tex->GetName())));
	if (CurrentBrush.IsValid())
	{
		BackgroundImage->SetImage(CurrentBrush.Get());
		BackgroundImage->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

void SClcLoadingScreenWidget::AdvanceBackground()
{
	if (Backgrounds.Num() == 0)
	{
		return;
	}
	CurrentBgIndex = (CurrentBgIndex + 1) % Backgrounds.Num();
	LoadBackgroundAt(CurrentBgIndex);
}

FString SClcLoadingScreenWidget::PickRandomTip() const
{
	// 配置非空用配置，否则用 C++ 内置默认池
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	const TArray<FString>* Source = (DS && DS->LoadingTips.Num() > 0) ? &DS->LoadingTips : &GetDefaultTips();
	if (Source->Num() == 0)
	{
		return FString();
	}
	return (*Source)[FMath::RandRange(0, Source->Num() - 1)];
}
