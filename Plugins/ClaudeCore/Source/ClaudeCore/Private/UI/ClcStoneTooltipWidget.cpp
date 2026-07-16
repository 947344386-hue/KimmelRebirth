// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcStoneTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

// ---- Tick：tip 跟着鼠标走 ----

void UClcStoneTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!AnchorWidget.IsValid())
	{
		Hide();
		return;
	}

	// 鼠标离开 cell 矩形 → Hide（替代不可靠的 NativeOnMouseLeave）
	const FGeometry& AnchorGeo = AnchorWidget->GetCachedGeometry();
	const FVector2D CellSize = AnchorGeo.GetLocalSize();
	const FVector2D LocalMouse = AnchorGeo.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
	if (LocalMouse.X < 0.0f || LocalMouse.X > CellSize.X ||
		LocalMouse.Y < 0.0f || LocalMouse.Y > CellSize.Y)
	{
		Hide();
		return;
	}

	// tip 位置 = 鼠标视口坐标 + Offset，超出屏幕边界则翻到鼠标另一侧
	if (APlayerController* PC = GetOwningPlayer())
	{
		float MouseX = 0.0f, MouseY = 0.0f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			const FVector2D Mouse(MouseX, MouseY);
			FVector2D TipPos = Mouse + TooltipOffset;

			// 拿视口尺寸 + tip 自身尺寸做边界检测
			FVector2D TipSize = MyGeometry.GetLocalSize();
			if (TipSize.IsNearlyZero())
			{
				TipSize = GetDesiredSize();
			}
			FVector2D ViewportSize = FVector2D::ZeroVector;
			if (UGameViewportClient* GVC = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
			{
				GVC->GetViewportSize(ViewportSize);
			}

			if (ViewportSize.X > 0.0f && ViewportSize.Y > 0.0f)
			{
				// 右边超出 → 翻到鼠标左边
				if (TipPos.X + TipSize.X > ViewportSize.X)
				{
					TipPos.X = Mouse.X - TipSize.X - TooltipOffset.X;
				}
				// 下边超出 → 翻到鼠标上方
				if (TipPos.Y + TipSize.Y > ViewportSize.Y)
				{
					TipPos.Y = Mouse.Y - TipSize.Y - TooltipOffset.Y;
				}
				// 兜底不跑出负坐标
				TipPos.X = FMath::Max(TipPos.X, 0.0f);
				TipPos.Y = FMath::Max(TipPos.Y, 0.0f);
			}

			SetPositionInViewport(TipPos);
		}
	}
}

// ---- C++ 默认实现：直接 SetText 四个 TextBlock ----

void UClcStoneTooltipWidget::ShowTooltip_Implementation(const FClcStoneTooltipInfo& Info)
{
	if (NameText)
	{
		NameText->SetText(FText::FromString(Info.DisplayName));
	}

	if (OriginText)
	{
		OriginText->SetText(FText::FromString(Info.Origin));
	}

	if (ShellOrGradeText)
	{
		const FString& ShellOrGrade = Info.bOpenedToJade ? Info.GradeText : Info.ShellName;
		ShellOrGradeText->SetText(FText::FromString(ShellOrGrade));
	}

	if (ValueText)
	{
		FString ValueStr;
		FLinearColor Color;

		if (Info.CurrentValue > Info.PurchasePrice)
		{
			ValueStr = FString::Printf(TEXT("价格：%d（%d↑）"),
				Info.CurrentValue, Info.PurchasePrice);
			Color = FLinearColor::Green;
		}
		else if (Info.CurrentValue < Info.PurchasePrice)
		{
			ValueStr = FString::Printf(TEXT("价格：%d（%d↓）"),
				Info.CurrentValue, Info.PurchasePrice);
			Color = FLinearColor::Red;
		}
		else
		{
			ValueStr = FString::Printf(TEXT("价格：%d（%d=）"),
				Info.CurrentValue, Info.PurchasePrice);
			Color = FLinearColor::White;
		}

		ValueText->SetText(FText::FromString(ValueStr));
		ValueText->SetColorAndOpacity(FSlateColor(Color));
	}
}

// ---- 创建 + 初始定位 + 显示 ----

UClcStoneTooltipWidget* UClcStoneTooltipWidget::ShowTooltipNextTo(
	UObject* WorldContextObject,
	UWidget* AnchorWidget,
	const FClcStoneTooltipInfo& Info,
	TSubclassOf<UClcStoneTooltipWidget> TooltipClass)
{
	if (!WorldContextObject || !AnchorWidget || !TooltipClass)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	UClcStoneTooltipWidget* Tooltip = CreateWidget<UClcStoneTooltipWidget>(World, TooltipClass);
	if (!Tooltip)
	{
		return nullptr;
	}

	Tooltip->SetAnchor(AnchorWidget);
	Tooltip->AddToViewport(1000); // tooltip 层级最高，避免被其他 UI 遮挡

	// 初始定位：鼠标位置 + Offset（NativeTick 每帧接管）
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		float MouseX = 0.0f, MouseY = 0.0f;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			Tooltip->SetPositionInViewport(FVector2D(MouseX, MouseY) + Tooltip->TooltipOffset);
		}
	}

	Tooltip->ShowTooltip(Info);
	return Tooltip;
}

void UClcStoneTooltipWidget::Hide()
{
	RemoveFromParent();
}
