// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcMerchantBubbleWidget.h"
#include "UI/ClcUILayers.h"
#include "UI/ClcMerchantOffScreenArrowWidget.h"
#include "Components/TextBlock.h"
#include "Components/SceneComponent.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/ClcBackpackSubsystem.h"

void UClcMerchantBubbleWidget::SetAnchor(USceneComponent* InAnchorComponent, const FVector& InLocalOffset)
{
	AnchorComponent = InAnchorComponent;
	AnchorLocalOffset = InLocalOffset;
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UClcMerchantBubbleWidget::SetSimulatedPerspective(const FClcMerchantUISimulatedPerspectiveSettings& InSettings)
{
	SimulatedPerspective = InSettings;
}

void UClcMerchantBubbleWidget::SetOffScreenSettings(bool bEnabled, float EdgeMargin, float OffScale)
{
	bOffScreenEnabled = bEnabled;
	OffScreenEdgeMargin = FMath::Max(0.f, EdgeMargin);
	OffScreenScale = FMath::Max(0.1f, OffScale);

	// 仅口头气泡启用：创建独立屏幕空间箭头 Widget 并挂到视口（销毁时在 NativeDestruct 移除）
	if (bEnabled && !ArrowWidget)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			ArrowWidget = CreateWidget<UClcMerchantOffScreenArrowWidget>(PC, UClcMerchantOffScreenArrowWidget::StaticClass());
			if (ArrowWidget)
			{
				ArrowWidget->AddToViewport(FClcUIZOrder::MerchantArrow);
				ArrowWidget->Hide();
			}
		}
	}
}

void UClcMerchantBubbleWidget::NativeDestruct()
{
	if (ArrowWidget)
	{
		ArrowWidget->RemoveFromParent();
		ArrowWidget = nullptr;
	}
	Super::NativeDestruct();
}

void UClcMerchantBubbleWidget::SetBubbleText(const FText& Text)
{
	if (BubbleTextBlock)
	{
		BubbleTextBlock->SetText(Text);
	}
}

void UClcMerchantBubbleWidget::SetSecondaryText(const FText& Text)
{
	if (!SecondaryTextBlock) return;

	if (Text.IsEmpty())
	{
		SecondaryTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		SecondaryTextBlock->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SecondaryTextBlock->SetText(Text);
	}
}

void UClcMerchantBubbleWidget::UpdateScreenPosition()
{
	if (!AnchorComponent.IsValid())
	{
		SetVisibility(ESlateVisibility::Hidden);
		if (ArrowWidget) ArrowWidget->Hide();
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		if (ArrowWidget) ArrowWidget->Hide();
		return;
	}

	const FVector WorldPos = AnchorComponent->GetComponentTransform().TransformPositionNoScale(AnchorLocalOffset);
	FVector2D ScreenPos;
	const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true);

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		if (ArrowWidget) ArrowWidget->Hide();
		return;
	}

	const FVector2D ScreenCenter(static_cast<float>(ViewportX) * 0.5f, static_cast<float>(ViewportY) * 0.5f);

	// ===== 未启用离屏指示器（鹰眼洞察）：在屏跟踪、离屏隐藏（旧行为） =====
	if (!bOffScreenEnabled)
	{
		const bool bOnScreen = bProjected
			&& ScreenPos.X >= 0.0f && ScreenPos.X <= static_cast<float>(ViewportX)
			&& ScreenPos.Y >= 0.0f && ScreenPos.Y <= static_cast<float>(ViewportY);
		if (!bOnScreen)
		{
			SetVisibility(ESlateVisibility::Hidden);
			return;
		}

		float Scale = 1.0f;
		if (SimulatedPerspective.bEnabled && SimulatedPerspective.FarDistance > SimulatedPerspective.NearDistance)
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
			Scale = FMath::GetMappedRangeValueClamped(
				FVector2D(SimulatedPerspective.NearDistance, SimulatedPerspective.FarDistance),
				FVector2D(SimulatedPerspective.NearScale, SimulatedPerspective.FarScale),
				FVector::Distance(CameraLocation, WorldPos));
		}
		SetRenderScale(FVector2D(Scale));

		if (GetVisibility() == ESlateVisibility::Hidden)
		{
			SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}

		if (BackpackOpenClampYFraction > 0.0f && PC->GetLocalPlayer())
		{
			if (UClcBackpackSubsystem* BP = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>())
			{
				if (BP->IsBackpackOpen())
				{
					ScreenPos.Y = FMath::Min(ScreenPos.Y, static_cast<float>(ViewportY) * BackpackOpenClampYFraction);
				}
			}
		}

		SetPositionInViewport(ScreenPos);
		return;
	}

	// ===== 启用离屏指示器（口头气泡）：始终钳制在屏内，离屏显示独立箭头 =====
	SetRenderScale(FVector2D(OffScreenScale));

	// 气泡视觉尺寸（布局期望 × 离屏缩放）；几何未就绪时用兜底
	FVector2D Desired = GetDesiredSize();
	if (Desired.X < 1.f || Desired.Y < 1.f)
	{
		const FVector2D Cached = GetCachedGeometry().GetLocalSize();
		Desired = (Cached.X > 1.f && Cached.Y > 1.f) ? Cached : FVector2D(160.f, 60.f);
	}
	const float HalfBW = Desired.X * OffScreenScale * 0.5f;
	const float HalfBH = Desired.Y * OffScreenScale * 0.5f;

	// Margin = 气泡外缘到屏幕边缘的间隙；气泡完整在屏内，箭头落在此间隙正中
	const float Margin = FMath::Max(0.f, OffScreenEdgeMargin);
	// 安全区半幅：气泡中心可移动范围（保证气泡完整在屏内 + 留 Margin 间隙）
	const float HalfW = FMath::Max(1.f, ScreenCenter.X - HalfBW - Margin);
	const float HalfH = FMath::Max(1.f, ScreenCenter.Y - HalfBH - Margin);

	FVector2D Clamped;
	FVector2D DirN(1.f, 0.f);
	bool bOffScreen = false;

	if (!bProjected)
	{
		// 相机背后：ProjectWorldToScreen 给出镜像投影点，方向取反指向正确边缘
		FVector2D D = -(ScreenPos - ScreenCenter);
		if (D.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			SetVisibility(ESlateVisibility::Hidden);
			if (ArrowWidget) ArrowWidget->Hide();
			return;
		}
		DirN = D.GetSafeNormal();
		const float AbsDx = FMath::Abs(D.X);
		const float AbsDy = FMath::Abs(D.Y);
		const float TX = AbsDx > KINDA_SMALL_NUMBER ? (HalfW / AbsDx) : FLT_MAX;
		const float TY = AbsDy > KINDA_SMALL_NUMBER ? (HalfH / AbsDy) : FLT_MAX;
		Clamped = ScreenCenter + D * FMath::Min(TX, TY);
		bOffScreen = true;
	}
	else
	{
		// 相机前：把商人投影点钳到安全区——气泡随商人连续移动，到边缘平滑停下（无跳变=不闪帧）
		Clamped.X = FMath::Clamp(ScreenPos.X, ScreenCenter.X - HalfW, ScreenCenter.X + HalfW);
		Clamped.Y = FMath::Clamp(ScreenPos.Y, ScreenCenter.Y - HalfH, ScreenCenter.Y + HalfH);
		// 箭头方向 = 气泡中心指向商人（出屏时指向屏外）；在屏内时此向量很小但不显示箭头
		DirN = ScreenPos - Clamped;
		if (!DirN.IsNearlyZero(KINDA_SMALL_NUMBER))
		{
			DirN.Normalize();
		}
		else
		{
			DirN = (ScreenPos - ScreenCenter).GetSafeNormal();
			if (DirN.IsNearlyZero()) DirN = FVector2D(1.f, 0.f);
		}
		// 出屏判定以真实视口边界为准（商人移出屏幕才显箭头）
		bOffScreen = ScreenPos.X < 0.f || ScreenPos.X > static_cast<float>(ViewportX)
			|| ScreenPos.Y < 0.f || ScreenPos.Y > static_cast<float>(ViewportY);
	}

	if (GetVisibility() == ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// 背包打开时面板会盖住中下部——把气泡 Y 钳到屏幕上半区，避免被遮
	if (BackpackOpenClampYFraction > 0.0f && PC->GetLocalPlayer())
	{
		if (UClcBackpackSubsystem* BP = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>())
		{
			if (BP->IsBackpackOpen())
			{
				Clamped.Y = FMath::Min(Clamped.Y, static_cast<float>(ViewportY) * BackpackOpenClampYFraction);
			}
		}
	}

	SetPositionInViewport(Clamped);

	// 离屏箭头（独立屏幕空间 Widget）：落「气泡外缘 ↔ 屏幕边缘」间隙正中，按方位旋转
	if (ArrowWidget)
	{
		if (bOffScreen)
		{
			const float HalfBubbleAlongDir = HalfBW * FMath::Abs(DirN.X) + HalfBH * FMath::Abs(DirN.Y);
			FVector2D ArrowPos = Clamped + DirN * (HalfBubbleAlongDir + Margin * 0.5f);
			// 角落方位下上方公式会让箭头越出屏幕（沿对角线到屏边距离短于轴向）——
			// 按箭头自身半尺寸钳制，保证箭头完整在屏内；轴向边缘不受影响。
			float ArrowHalf = ArrowWidget->GetDesiredSize().GetMax() * 0.5f;
			if (ArrowHalf < 4.f) ArrowHalf = 16.f; // 几何未就绪兜底
			const float ArrowPad = ArrowHalf + 2.f;
			ArrowPos.X = FMath::Clamp(ArrowPos.X, ArrowPad, static_cast<float>(ViewportX) - ArrowPad);
			ArrowPos.Y = FMath::Clamp(ArrowPos.Y, ArrowPad, static_cast<float>(ViewportY) - ArrowPad);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DirN.Y, DirN.X));
			ArrowWidget->ShowAt(ArrowPos, AngleDeg);
		}
		else
		{
			ArrowWidget->Hide();
		}
	}
}
