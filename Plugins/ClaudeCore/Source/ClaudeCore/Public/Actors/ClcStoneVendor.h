// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneVendor.generated.h"

class UStaticMeshComponent;
class UClcInteractionIndicator;
class UClcBackpackSubsystem;

/**
 * 出售入口 Actor——玩家走近交互，打开背包选石头出售。
 * C++ 提供交互+出售逻辑，蓝图继承定制 Mesh/音效/特效。
 *
 * 交互流程：
 *   1. 走近 → InteractionIndicator 检测（状态 0=隐藏 / 1=范围内 / 2=瞄准）
 *   2. 按交互键 → OnInteract → EnterSellMode（打开背包 + 绑定 OnStoneSelected）
 *   3. 点石头 → OnStoneSelectedForSale（CalculateSalePrice + RemoveStone + AddGold）
 *   4. 再按交互键 或 背包空 → ExitSellMode（解绑 + 关背包）
 *
 * 蓝图定制：VendorMesh（外观）、PromptText（提示）、OnStoneSold/OnEnter/OnExit（音效特效）
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	/** 出售点外观 Mesh——蓝图继承后定制（柜台、NPC、招牌等） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VendorMesh;

	/** 交互指示器——自动检测玩家范围 + 摄像机瞄准，三级状态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	// ---- 配置 ----

	/** 交互提示文字 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FText PromptText = FText::FromString(TEXT("按 F 出售石头"));

	/** 交互半径（运行时同步到 InteractionIndicator） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "100.0"))
	float InteractionRadius = 300.0f;

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

	/** 缓存的背包子系统（出售模式期间有效） */
	UPROPERTY()
	UClcBackpackSubsystem* CachedBackpack = nullptr;

	/** 是否处于出售模式 */
	bool bInSellMode = false;
};
