// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Tools/ClcStoneTool.h"
#include "ClcToolRepairStation.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UClcInteractionIndicator;

/**
 * 工具修理站 —— 可放置的通用蓝图 Actor。
 *
 * 玩家走进范围按 F 键 → 支付金币 → 恢复配置的工具体系的全部耐久。
 *
 * 蓝图使用：
 *   1. 在 Content 中创建继承此类的 BP（如 BP_ToolRepairStation）
 *   2. 设置 StationMesh（修理站视觉模型）
 *   3. 在 RepairableTools 中勾选要修复的工具类型（开窗器/手电筒，可多选）
 *   4. 设置 RepairCost（金币消耗）
 *   5. 拖入关卡
 */
UCLASS(Blueprintable, ClassGroup = (Clc))
class CLAUDECORE_API AClcToolRepairStation : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcToolRepairStation();

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

	/** 获取当前是否有工具需要修复（供 BP 查询） */
	UFUNCTION(BlueprintCallable, Category = "ClcRepairStation")
	bool HasToolsNeedingRepair() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* StationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	// ---- 配置 ----

	/** 可修复的工具类型（多选：开窗器=1，手电筒=2） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RepairStation|Config", meta = (Bitmask, BitmaskEnum = "EClcRepairableTool"))
	int32 RepairableTools = 3; // 默认两种都修复

	/** 修复消耗的金币数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RepairStation|Config", meta = (ClampMin = "0"))
	int32 RepairCost = 300;

	/** 交互触发距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RepairStation|Config", meta = (ClampMin = "50.0"))
	float InteractionRadius = 300.0f;

	/** 交互键 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RepairStation|Config")
	FKey EnterKey = FKey("F");

	/** 交互提示文本（留空则按 RepairableTools 自动生成，如 "按 F 修复开窗器、手电筒"） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RepairStation|Config")
	FText InteractionPrompt;

	// ---- 蓝图可覆写事件 ----

	UFUNCTION(BlueprintNativeEvent, Category = "RepairStation|Events")
	void OnRepairSuccess(int32 CostPaid);

	UFUNCTION(BlueprintNativeEvent, Category = "RepairStation|Events")
	void OnRepairFailed_NotEnoughGold(int32 RequiredGold, int32 CurrentGold);

	UFUNCTION(BlueprintNativeEvent, Category = "RepairStation|Events")
	void OnRepairFailed_NoToolsNeedRepair();

private:
	/** 执行修复操作 */
	void ExecuteRepair(APlayerController* PC);

	/** 按 RepairableTools 位掩码生成中文工具名串（如 "开窗器、手电筒"） */
	FString BuildToolNamesString() const;

	/** 生成交互提示文本（优先用 InteractionPrompt，为空则自动生成） */
	FText BuildInteractionPrompt() const;

	/** 交互组件中心球扫是否命中本站（复用 UClcInteractionComponent::GetLookedAtActor） */
	bool IsLookedAtByPlayer() const;

	// ---- 重叠 ----

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ---- 按键轮询 ----

	/** 按键边沿检测（自维护，避免输入模式切换重置 WasInputKeyJustPressed） */
	bool bEnterKeyPrev = false;
	bool bPlayerInRange = false;

	/** 缓存的玩家控制器 */
	TWeakObjectPtr<APlayerController> CachedPC;

	/** 按键提示句柄 */
	int32 RepairPromptHandle = 0;
};