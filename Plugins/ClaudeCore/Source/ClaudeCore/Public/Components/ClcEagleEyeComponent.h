// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcEagleEyeComponent.generated.h"

class UClcEagleEyeConfig;
class UClcStoneMarketSubsystem;
class AClcEnergyBall;
class AClcStoneStall;

/**
 * 鹰眼能力——挂在Character上，按技能键激活
 * 扫描附近摊位，生成能量球指示含金量
 */
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcEagleEyeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcEagleEyeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 激活鹰眼 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	void ActivateEagleEye();

	/** 是否在鹰眼激活状态 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	bool IsEagleEyeActive() const { return bActive; }

	/** 是否在冷却中 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	bool IsOnCooldown() const { return bCoolingDown; }

	/** 获取剩余激活时间 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	float GetRemainingActiveTime() const { return ActiveTimer; }

	/** 获取剩余冷却时间 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	float GetRemainingCooldownTime() const { return CooldownTimer; }

	// ---- 内部 ----
	void OnStallRegistered(AClcStoneStall* Stall);
	void OnStallUnregistered(AClcStoneStall* Stall);

protected:
	virtual void BeginPlay() override;

private:
	void InitializeConfig();
	void UpdateBalls();
	void SpawnBall(AClcStoneStall* Stall);
	void DestroyBall(AClcStoneStall* Stall);
	void DestroyAllBalls();
	float CalculateStallValue(AClcStoneStall* Stall) const;
	float MapScaleToValue(float Value) const;

	// ---- 配置 ----
	UPROPERTY()
	UClcEagleEyeConfig* Config;

	// ---- 状态 ----
	bool bActive = false;
	bool bCoolingDown = false;
	float ActiveTimer = 0.0f;
	float CooldownTimer = 0.0f;

	// ---- 能量球缓存 ----
	UPROPERTY()
	TMap<AClcStoneStall*, AClcEnergyBall*> EnergyBalls;

	// ---- 市场子系统引用 ----
	UPROPERTY()
	UClcStoneMarketSubsystem* MarketSubsystem;

	/** 能量球Actor类 */
	UPROPERTY()
	TSubclassOf<AClcEnergyBall> EnergyBallClass;

	/** 扫描摊位Timer */
	float ScanTimer = 0.0f;
	static constexpr float SCAN_INTERVAL = 0.5f;
};
