// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcInteractionIndicator.generated.h"

class UClcInteractionWidget;
class UWidgetComponent;

/**
 * AAA风格交互指示器——挂在任意Actor上即获得三级交互指示
 * 隐藏 → 在范围内(外圈环) → 摄像机瞄准(外圈+内点)
 */
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcInteractionIndicator : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcInteractionIndicator();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 交互半径 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction")
	float InteractionRadius = 200.0f;

	/** 小白点Widget相对于Actor的位置偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction")
	FVector WidgetOffset = FVector(0.0f, 0.0f, 50.0f);

	/** 获取当前交互状态：0=隐藏, 1=在范围内, 2=选中 */
	UFUNCTION(BlueprintCallable, Category = "ClcInteraction")
	int32 GetInteractionState() const { return CurrentState; }

protected:
	virtual void BeginPlay() override;

private:
	void UpdateInteractionState();

	UPROPERTY()
	UWidgetComponent* WidgetComp;

	UPROPERTY()
	UClcInteractionWidget* InteractionWidget;

	/** 当前状态 0=隐藏, 1=在范围内, 2=选中 */
	int32 CurrentState = 0;

	/** Widget类 */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	TSubclassOf<UClcInteractionWidget> WidgetClass;
};
