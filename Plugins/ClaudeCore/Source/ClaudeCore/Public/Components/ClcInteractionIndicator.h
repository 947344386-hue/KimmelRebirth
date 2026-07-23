// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcInteractionIndicator.generated.h"

class UClcInteractionWidget;
class UWidgetComponent;

/** 范围选中查询委托——Owner 绑定后返回 true=可选中(显示内点), false=仅范围内(显示外圈) */
DECLARE_DYNAMIC_DELEGATE_RetVal(bool, FClcOnQueryCanSelect);

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

	/** 瞄准模式球扫半径——把单条射线改成球扫，放宽命中。
	 *  越肩偏高视角下不必把摄像机压很低就能选中；球比线粗，不易被石头前缘/摊位边/地面遮挡。
	 *  0 = 退回细射线（兼容旧行为）。石头默认 25，可在 BP_Stone 调。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction", meta = (ClampMin = 0.0f))
	float AimSweepRadius = 25.0f;

	/** 小白点Widget相对于Actor的位置偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction")
	FVector WidgetOffset = FVector(0.0f, 0.0f, 50.0f);

	/**
	 * 范围选中模式——true 时不再要求摄像机瞄准，范围内即根据委托决定选中/范围内。
	 * 默认 false（保持摄像机瞄准逻辑）。
	 * 工作台/出售台等"走近就能交互"的 Actor 开启此项。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction")
	bool bSelectByProximity = false;

	/** 范围选中查询委托——bSelectByProximity=true 时每帧调用，返回 true=选中, false=仅范围内。
	 *  未绑定则视为 true（纯距离选中）。Owner 绑定此委托实现"背包有石头才选中"等业务条件。 */
	UPROPERTY()
	FClcOnQueryCanSelect OnQueryCanSelect;

	/** 强制隐藏——true 时小白点立即隐藏并停止更新（工作台进开窗模式时设 true 避免碍事） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClcInteraction")
	bool bHidden = false;

	/** 获取当前交互状态：0=隐藏, 1=在范围内, 2=选中 */
	UFUNCTION(BlueprintCallable, Category = "ClcInteraction")
	int32 GetInteractionState() const { return CurrentState; }

	/** 由角色侧 UClcInteractionComponent 统一驱动视觉态——收敛后各 Indicator 不再各自球扫自检。
	 *  记录最近驱动时刻；若超过 0.5s 无人驱动（如角色未挂中心组件），本组件 Tick 回退到自检。 */
	UFUNCTION(BlueprintCallable, Category = "ClcInteraction")
	void ApplyControllerState(int32 NewState);

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

	/** 最近一次被中心组件 ApplyControllerState 驱动的世界时间秒；超过 0.5s 未驱动则 Tick 回退自检。 */
	float LastDrivenTime = -1.0f;

	/** Widget类 */
	UPROPERTY(EditAnywhere, Category = "Interaction")
	TSubclassOf<UClcInteractionWidget> WidgetClass;
};
