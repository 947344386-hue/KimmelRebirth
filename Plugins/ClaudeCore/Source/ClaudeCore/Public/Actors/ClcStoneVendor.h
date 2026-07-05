// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneVendor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class USceneComponent;
class UClcInteractionIndicator;
class UClcBackpackSubsystem;

/**
 * 出售入口 Actor——玩家走进范围 + 背包有石头即可按 F 出售。
 * C++ 提供交互+出售逻辑，蓝图继承定制 Mesh/音效/特效。
 *
 * 交互流程（与工作台一致：TriggerSphere 检测 + Tick F 键）：
 *   1. 走进 TriggerSphere → InteractionIndicator 范围内（背包有石头→选中状态）
 *   2. 按 F → OnInteract → EnterSellMode（打开背包 + 绑定 OnStoneSelected）
 *   3. 点石头 → OnStoneSelectedForSale（CalculateSalePrice + RemoveStone + AddGold）
 *   4. 再按 F 或 背包空 或 离开范围 → ExitSellMode（解绑 + 关背包）
 *
 * 蓝图定制：VendorMesh（外观）、PromptText（提示）、InteractionRadius、EnterKey
 *           OnStoneSold/OnEnter/OnExit（音效特效）
 */
UCLASS(Blueprintable, ClassGroup = (Clc))
class CLAUDECORE_API AClcStoneVendor : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcStoneVendor();

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

	/** 查询当前是否处于出售模式 */
	UFUNCTION(BlueprintCallable, Category = "ClcVendor")
	bool IsInSellMode() const { return bInSellMode; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	/** 无缩放根——VendorMesh/TriggerSphere/Indicator 都挂这下面，避免 BP 缩放污染 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* VendorRoot;

	/** 出售点外观 Mesh——蓝图继承后定制（柜台、NPC、招牌等） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VendorMesh;

	/** 范围触发器——检测玩家进出范围（只响应 Pawn Overlap） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	/** 交互指示器——范围内+背包有石头→选中状态（bSelectByProximity 模式） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	// ---- 配置 ----

	/** 交互提示文字 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FText PromptText = FText::FromString(TEXT("按 F 出售石头"));

	/** 交互半径（运行时同步到 TriggerSphere 和 InteractionIndicator） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "100.0"))
	float InteractionRadius = 300.0f;

	/** 进入出售模式的按键（默认 F） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey EnterKey = FKey("F");

	/** 退出出售模式的按键（默认 Escape） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey ExitKey = FKey("Escape");

	// ---- 蓝图事件（蓝图覆写以播放音效/特效/动画） ----

	/** 石头出售成功——蓝图可播音效/特效/粒子 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnStoneSold(const FClcStoneRuntimeData& StoneData, int32 SalePrice);

	/** 进入出售模式——蓝图可播进入动画/提示 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnEnterSellMode();

	/** 退出出售模式——蓝图可播退出动画 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnExitSellMode();

private:
	/** 背包选石回调——出售选中的石头 */
	UFUNCTION()
	void OnStoneSelectedForSale(int32 StoneIndex);

	/** 进入出售模式：打开背包 + 绑定委托 */
	void EnterSellMode(UClcBackpackSubsystem* Backpack);

	/** 退出出售模式：解绑委托 + 关闭背包 */
	void ExitSellMode();

	/** InteractionIndicator 委托——背包有石头返回 true（选中态），否则 false（范围内态） */
	UFUNCTION()
	bool QueryCanSelect();

	/** TriggerSphere overlap 回调 */
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 缓存的背包子系统（出售模式期间有效） */
	UPROPERTY()
	UClcBackpackSubsystem* CachedBackpack = nullptr;

	/** 范围内玩家（弱引用） */
	UPROPERTY()
	TWeakObjectPtr<APawn> PlayerInRange;

	/** 缓存的玩家控制器（范围期间有效） */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> CachedPC;

	/** 是否处于出售模式 */
	bool bInSellMode = false;
};
