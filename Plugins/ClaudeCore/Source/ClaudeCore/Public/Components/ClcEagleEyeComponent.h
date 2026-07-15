// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcEagleEyeComponent.generated.h"

class UClcEagleEyeConfig;
class UClcStoneMarketSubsystem;
class AClcStoneStall;
class AClcMerchant;

/**
 * 鹰眼能力——挂在 Character 上，按技能键激活
 * 激活时显示各摊位商人的内心独白气泡（诚实但隐晦），限时。
 * 结束后进入冷却。气泡不常驻，避免场景碎脏。
 */
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcEagleEyeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcEagleEyeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 激活鹰眼——显示商人气泡 */
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

protected:
	virtual void BeginPlay() override;

private:
	void InitializeConfig();

	/** 遍历摊位，开关商人气泡 */
	void ToggleMerchantBubbles(bool bShow);

	// ---- 配置 ----
	UPROPERTY()
	UClcEagleEyeConfig* Config;

	// ---- 状态 ----
	bool bActive = false;
	bool bCoolingDown = false;
	float ActiveTimer = 0.0f;
	float CooldownTimer = 0.0f;

	// ---- 市场子系统引用 ----
	UPROPERTY()
	UClcStoneMarketSubsystem* MarketSubsystem;
};
