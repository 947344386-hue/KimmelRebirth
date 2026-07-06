// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcStoneInfoWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

void UClcStoneInfoWidget::SetAnchor(AActor* InAnchor, const FVector& InWorldOffset)
{
	AnchorActor = InAnchor;
	AnchorWorldOffset = InWorldOffset;
}

void UClcStoneInfoWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!AnchorActor.IsValid())
	{
		// 锚点失效（Stone 被销毁但没走 HideInfoCard）→ 隐藏兜底，不 RemoveFromParent
		// （生命周期由 AClcStone 管，这里只防视觉残留）
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// 小白点世界位置 → 屏幕投影 → 定位
	// （小白点被摄像机选中才显示信息卡，投影一定在屏幕内，不用检查 ProjectWorldToScreen 返回值）
	const FVector WorldPos = AnchorActor->GetActorLocation() + AnchorWorldOffset;
	FVector2D ScreenPos;
	UGameplayStatics::ProjectWorldToScreen(PC, WorldPos, ScreenPos, true);
	SetPositionInViewport(ScreenPos + ScreenOffset);
}
