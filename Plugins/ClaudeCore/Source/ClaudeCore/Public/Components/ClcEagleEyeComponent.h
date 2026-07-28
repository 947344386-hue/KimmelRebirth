// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcEagleEyeComponent.generated.h"

class UClcEagleEyeConfig;
class UClcStoneMarketSubsystem;
class UPostProcessComponent;
class UMaterialInstanceDynamic;
class AClcStoneStall;
class AClcMerchant;

/**
 * 鹰眼能力——挂在 Character 上，按技能键激活。
 * 每次按键以玩家为中心 ScanRadius 范围判定，只刷新范围内商人的洞察 UI；
 * 每个商人独立残留计时，范围外商人按自己上次激活继续走至自然熄灭。
 * 玩家侧保留短 CD 防连点。
 * 扫描视觉用 ScanFX：按 Q 时创建后处理 + 动态材质实例，写入 Scan Start Location/Time，
 * 材质内部按游戏时间自动扩散并在 Scan Duration 后淡出，组件到点销毁后处理。
 */
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcEagleEyeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcEagleEyeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 激活鹰眼——以玩家为中心 ScanRadius 范围判定，刷新范围内每个商人的残留计时；CD 中无效 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	void ActivateEagleEye();

	/** 是否在冷却中 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	bool IsOnCooldown() const { return bCoolingDown; }

	/** 获取剩余冷却时间 */
	UFUNCTION(BlueprintCallable, Category = "ClcEagleEye")
	float GetRemainingCooldownTime() const { return CooldownTimer; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void InitializeConfig();
	bool InitializeScanEffect();
	void StartOrRestartScanPulse(const FVector& Center);
	void CleanupScanEffect();
	bool IsInExclusiveFlow() const;

	// ---- 配置 ----
	UPROPERTY()
	UClcEagleEyeConfig* Config;

	// ---- 玩家侧 CD（防连点）----
	bool bCoolingDown = false;
	float CooldownTimer = 0.0f;

	// ---- 市场子系统引用 ----
	UPROPERTY()
	UClcStoneMarketSubsystem* MarketSubsystem;

	// ---- 扫描视觉（ScanFX MID 方案）----
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> ScanPostProcessComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScanMID;

	bool bScanActive = false;
	float ScanEndTimer = 0.0f;

	/** 按键提示句柄：常驻 Q（鹰眼），TickComponent 首帧 PC 就绪后注册，EndPlay 注销 */
	int32 EagleEyePromptHandle = 0;
};
