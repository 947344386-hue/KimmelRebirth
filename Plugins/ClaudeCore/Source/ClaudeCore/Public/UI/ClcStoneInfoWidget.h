// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneInfoWidget.generated.h"

/**
 * 石头信息卡片——摄像机瞄准石头时显示于小白点正上方
 *
 * 位置由 C++ NativeTick 驱动：每帧把锚点世界位置投影到屏幕，SetPositionInViewport。
 * UMG 端 root 用什么都行（SizeBox/Border/Canvas），不用靠 anchors 定位。
 */
UCLASS()
class CLAUDECORE_API UClcStoneInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcStoneInfo")
	void ShowInfo(const FClcStoneRuntimeData& StoneData);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "ClcStoneInfo")
	void HideInfo();

	/** 设锚点——信息卡每帧跟随此 Actor 的 (ActorLocation + WorldOffset) 屏幕投影 */
	void SetAnchor(AActor* InAnchor, const FVector& InWorldOffset);

	/** 立即将锚点世界位置投影到视口；创建后调用以避免首帧显示在左上角。 */
	void UpdateScreenPosition();

	/** 信息卡相对小白点屏幕投影的像素偏移（BP Class Defaults 里配，默认正上方 50px） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcStoneInfo")
	FVector2D ScreenOffset = FVector2D(0.0f, -50.0f);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	TWeakObjectPtr<AActor> AnchorActor;
	FVector AnchorWorldOffset = FVector::ZeroVector;
};
