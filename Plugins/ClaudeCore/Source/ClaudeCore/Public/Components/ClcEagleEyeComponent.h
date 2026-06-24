// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcEagleEyeComponent.generated.h"
class UClcEagleEyeConfig; class UClcStoneMarketSubsystem; class AClcEnergyBall; class AClcStoneStall;
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcEagleEyeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UClcEagleEyeComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UFUNCTION(BlueprintCallable) void ActivateEagleEye();
	UFUNCTION(BlueprintCallable) bool IsEagleEyeActive() const { return bActive; }
	UFUNCTION(BlueprintCallable) bool IsOnCooldown() const { return bCoolingDown; }
	UFUNCTION(BlueprintCallable) float GetRemainingActiveTime() const { return ActiveTimer; }
	UFUNCTION(BlueprintCallable) float GetRemainingCooldownTime() const { return CooldownTimer; }
	void OnStallRegistered(AClcStoneStall* Stall); void OnStallUnregistered(AClcStoneStall* Stall);
protected: virtual void BeginPlay() override;
private:
	void InitializeConfig(); void UpdateBalls(); void SpawnBall(AClcStoneStall* Stall);
	void DestroyBall(AClcStoneStall* Stall); void DestroyAllBalls();
	float CalculateStallValue(AClcStoneStall* Stall) const; float MapScaleToValue(float Value) const;
	UPROPERTY() UClcEagleEyeConfig* Config;
	bool bActive=false; bool bCoolingDown=false; float ActiveTimer=0; float CooldownTimer=0;
	UPROPERTY() TMap<AClcStoneStall*,AClcEnergyBall*> EnergyBalls;
	UPROPERTY() UClcStoneMarketSubsystem* MarketSubsystem;
	UPROPERTY() TSubclassOf<AClcEnergyBall> EnergyBallClass;
	float ScanTimer=0; static constexpr float SCAN_INTERVAL=0.5f;
};