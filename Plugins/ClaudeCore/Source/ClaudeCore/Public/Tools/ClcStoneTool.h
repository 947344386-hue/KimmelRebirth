// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcStoneTool.generated.h"

class AClcOpeningStone;
class UStaticMeshComponent;

/** Workbench 每帧做完鼠标 LineTrace 后传给工具的上下文 */
USTRUCT(BlueprintType)
struct FClcToolTraceInfo
{
	GENERATED_BODY()

	/** 射线是否命中了石头表面 */
	UPROPERTY(BlueprintReadOnly)
	bool bHasHit = false;

	/** 命中点（世界坐标） */
	UPROPERTY(BlueprintReadOnly)
	FVector HitPoint = FVector::ZeroVector;

	/** 命中点表面法线（世界坐标） */
	UPROPERTY(BlueprintReadOnly)
	FVector SurfaceNormal = FVector::UpVector;

	/** 鼠标射线起点（世界坐标） */
	UPROPERTY(BlueprintReadOnly)
	FVector RayOrigin = FVector::ZeroVector;

	/** 鼠标射线方向（归一化，世界坐标） */
	UPROPERTY(BlueprintReadOnly)
	FVector RayDirection = FVector::ForwardVector;
};

/**
 * 石头工具基类——开窗器、手电筒等的公共接口。
 * 由 Workbench Spawn，每帧通过 OnUpdate 传入鼠标射线命中信息。
 * 子类实现具体逻辑（打磨、光照、耐久消耗等）。
 *
 * 子类蓝图设置 ToolMesh 的 StaticMesh 即可实现视觉表现，
 * 后续客户端表现层（动画、特效、音效）在各工具 BP 中单独实现。
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class CLAUDECORE_API AClcStoneTool : public AActor
{
	GENERATED_BODY()

public:
	AClcStoneTool();

	// ---- 生命周期 ----

	/** 帧更新——指数平滑工具位姿（OnUpdate 设定目标，这里追赶） */
	virtual void Tick(float DeltaTime) override;

	/** 绑定目标石头（Spawn 后由 Workbench 调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcTool")
	virtual void Initialize(AClcOpeningStone* Stone);

	/** 工具被选中（切换到此模式时调用）——BP 可覆写播放装备动画等 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcTool")
	void OnActivated();
	virtual void OnActivated_Implementation() {}

	/** 工具被取消选中（切换出此模式时调用）——BP 可覆写播放收起动画等 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcTool")
	void OnDeactivated();
	virtual void OnDeactivated_Implementation() {}

	// ---- 每帧更新 ----

	/** 每帧由 Workbench 调用——鼠标命中石头表面时定位工具 + 执行逻辑 */
	virtual void OnUpdate(const FClcToolTraceInfo& TraceInfo) {}

	/** 每帧由 Workbench 调用——鼠标未命中石头时 */
	virtual void OnUpdateIdle() {}

	/** 左键按下/松开——具体行为由子类定义 */
	virtual void OnLeftClick(bool bPressed) {}

	// ---- 平滑 ----

	/** 平滑速度系数（越大越快追目标，1=瞬间到位，0.05=慢速缓动） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool|Smooth", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SmoothSpeed = 0.2f;

	/** 位置平滑系数（分开控制，不设则用 SmoothSpeed） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool|Smooth", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SmoothSpeedLocation = 0.12f;

	/** 旋转平滑系数 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool|Smooth", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SmoothSpeedRotation = 0.18f;

	// ---- 耐久 ----

	UFUNCTION(BlueprintCallable, Category = "ClcTool")
	float GetDurabilityRatio() const { return MaxDurability > 0.0f ? CurrentDurability / MaxDurability : 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "ClcTool")
	bool IsBroken() const { return CurrentDurability <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "ClcTool")
	float GetCurrentDurability() const { return CurrentDurability; }

	UFUNCTION(BlueprintCallable, Category = "ClcTool")
	float GetMaxDurability() const { return MaxDurability; }

protected:
	/** 消耗耐久（自动 Clamp 到 0） */
	void ConsumeDurability(float Amount);

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* ToolRoot;

	/** 工具视觉 Mesh——预留空位，BP 子类在 Class Defaults 里设 StaticMesh */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ToolMesh;

	// ---- 配置 ----

	/** 最大耐久度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tool|Config")
	float MaxDurability = 100.0f;

	// ---- 运行时 ----

	/** 目标石头（操作对象） */
	UPROPERTY(BlueprintReadOnly, Category = "Tool|Runtime")
	AClcOpeningStone* TargetStone = nullptr;

	/** 当前耐久度（运行时从 MaxDurability 开始递减） */
	float CurrentDurability = 100.0f;

	/** OnUpdate 设定的目标位姿（Tick 里平滑追过去） */
	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	bool bHasTarget = false;
};
