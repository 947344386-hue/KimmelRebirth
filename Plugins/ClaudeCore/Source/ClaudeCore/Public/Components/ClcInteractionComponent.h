// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcInteractionComponent.generated.h"

class UClcInteractionWidget;
class UClcInteractionIndicator;

/**
 * 角色侧中心交互组件——挂在角色（Pawn）上，作为"玩家正在瞄谁"的唯一真相。
 *
 * 收敛前：每个交互物身上的 UClcInteractionIndicator 各自从摄像机位置球扫自检命中自己，
 * 商人 AClcMerchant 另有一条 5000 距离的独立球扫。两套 trace 容差/距离不一致，
 * 导致"Indicator 选中但商人细射线擦边没中"→ 嘴上气泡偶尔不显示种水叫卖。
 *
 * 收敛后：本组件每帧做唯一一条相机球扫，产出：
 *   - CurrentLookedAtActor：中心射线第一命中且为 IClcInteractable 的 Actor（无 in-range 门槛，
 *     供商人长距离瞄准反应，距离 LookDistance ≥ 旧 5000）。
 *   - CurrentSelectedActor：瞄中且在该 Actor 自身 InteractionRadius 内（aim 模式），
 *     或 proximity 模式下由该 Actor 的 OnQueryCanSelect 委托决定。
 *
 * 每个 InteractionRadius 仍由各交互物自己的 UClcInteractionIndicator 持有，本组件按各 Actor
 * 自身半径判 in-range——石头/工作台/回收商可各自配置不同交互距离。
 *
 * 副作用驱动：
 *   - 屏幕中心准星 Reticle（进范围显示、选中变反馈 + GetInteractionPrompt 文案）。
 *   - 各 UClcInteractionIndicator 的视觉态（ApplyControllerState 0/1/2）；各 Indicator 自身
 *     的相机 trace 降级为"无中心组件驱动时"的回退。
 */
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 中心瞄准球扫半径（射线粗细）——0 退回细射线。石头默认 25，与旧 Indicator 行为一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ClcInteraction", meta=(ClampMin=0.0f))
	float ReticleSweepRadius = 25.0f;

	/** 中心瞄准距离——覆盖商人长距离反应（≥旧 5000）与交互判定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ClcInteraction", meta=(ClampMin=100.0f))
	float LookDistance = 6000.0f;


	/** 屏幕中心准星 Widget 类（BP 子类排版，复用 UClcInteractionWidget）。
	 *  未指定则按约定路径加载 /Game/JadeBetting/UI/WBP_Reticle。资产未创建前准星不显示，
	 *  但中心 trace 与各 Indicator 驱动、商人收敛照常生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ClcInteraction")
	TSubclassOf<UClcInteractionWidget> ReticleWidgetClass;

	/** 当前"瞄中"的交互物（中心射线第一命中且为 IClcInteractable，无 in-range 门槛）。
	 *  供商人长距离瞄准反应读取，取代商人自身的 5000 球扫。 */
	UFUNCTION(BlueprintCallable, Category="ClcInteraction")
	AActor* GetLookedAtActor() const { return CurrentLookedAtActor.Get(); }

	/** 当前"选中"的交互物（瞄中且在其自身 InteractionRadius 内；proximity 模式由委托决定）。 */
	UFUNCTION(BlueprintCallable, Category="ClcInteraction")
	AActor* GetSelectedActor() const { return CurrentSelectedActor.Get(); }

	/** 外部通知本组件"当前进入了独占上下文"（背包/菜单等），本组件会隐藏选中提示 */
	void SetExclusiveContext(bool bExclusive);

	/** 统一 F 键路由：边沿检测 + 调用 OnInteract + 兜底 Toast */
	void HandleInteractInput();

protected:
	virtual void BeginPlay() override;

private:
	/** 单帧核心：一条中心球扫 + 收集附近交互物 + 驱动准星与各 Indicator。 */
	void UpdateInteraction();

	/** 应用准星态并推送 Prompt 文案。 */
	void SetReticleState(int32 State, const FText& Prompt);

	/** 准星 Widget（AddToViewport，锚定屏幕中心）。 */
	UPROPERTY()
	UClcInteractionWidget* ReticleWidget = nullptr;

	/** 中心射线第一命中的 IClcInteractable（无 in-range 门槛）。 */
	TWeakObjectPtr<AActor> CurrentLookedAtActor;

	/** 当前选中（aim 命中且 in-range，或 proximity 委托 true）。 */
	TWeakObjectPtr<AActor> CurrentSelectedActor;

	int32 CurrentReticleState = 0;

	/** 当前选中 Actor 的统一 F 键提示句柄（本组件全自动管理，各交互物不再各自维护） */
	int32 SelectedPromptHandle = 0;

	/** 交互物缓存——避免每帧 GetAllActorsWithInterface + 每物 FindComponentByClass（关卡变大后 CPU/GC 压力大）。
	 *  每秒重建一次，其余 tick 直接复用弱引用；新生成交互物最多 1 秒后纳入（静态摊位场景可接受）。 */
	TArray<TWeakObjectPtr<AActor>> CachedInteractables;
	TArray<TWeakObjectPtr<UClcInteractionIndicator>> CachedIndicators;
	double InteractableCacheRebuildTime = 0.0;

	// ---- 统一 F 键路由（替代各站点各自 Tick 中的 KeyPrompt 注册/注销 + 按键边沿检测） ----
	TWeakObjectPtr<AActor> LastSelectedActor;
	bool bInExclusiveContext = false;
	bool bInteractKeyPrev = false;
};
