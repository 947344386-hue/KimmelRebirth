// Copyright ClaudeCore. All Rights Reserved.
#include "Components/ClcEagleEyeComponent.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcEnergyBall.h"
#include "Engine/World.h"
UClcEagleEyeComponent::UClcEagleEyeComponent() { PrimaryComponentTick.bCanEverTick = true; }
void UClcEagleEyeComponent::BeginPlay() { Super::BeginPlay(); InitializeConfig(); MarketSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>(); EnergyBallClass = LoadClass<AClcEnergyBall>(nullptr, TEXT("/Game/JadeBetting/Blueprints/BP_EnergyBall.BP_EnergyBall_C")); }
void UClcEagleEyeComponent::InitializeConfig() { Config = LoadObject<UClcEagleEyeConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_EagleEyeConfig")); }
void UClcEagleEyeComponent::ActivateEagleEye() { if (bCoolingDown||!Config) return; bActive=true; ActiveTimer=Config->ActiveDuration; bCoolingDown=false; ScanTimer=0; UpdateBalls(); }
void UClcEagleEyeComponent::TickComponent(float DT, ELevelTick, FActorComponentTickFunction*) {
	Super::TickComponent(DT, ELevelTick(), nullptr);
	if (!Config) return;
	if (bActive) {
		ActiveTimer-=DT; ScanTimer-=DT;
		if (ScanTimer<=0) { ScanTimer=SCAN_INTERVAL; UpdateBalls(); }
		if (ActiveTimer<=0) { bActive=false; bCoolingDown=true; CooldownTimer=Config->CooldownDuration; DestroyAllBalls(); }
	}
	if (bCoolingDown&&(CooldownTimer-=DT)<=0) bCoolingDown=false;
}
void UClcEagleEyeComponent::UpdateBalls() {
	if (!MarketSubsystem||!EnergyBallClass) return;
	for (auto& SP : MarketSubsystem->GetStalls()) {
		AClcStoneStall* S = SP.Get(); if (!S) continue;
		float V = CalculateStallValue(S);
		if (AClcEnergyBall** E = EnergyBalls.Find(S)) { if (*E) (*E)->SetScaleValue(MapScaleToValue(V)); }
		else SpawnBall(S);
	}
}
void UClcEagleEyeComponent::SpawnBall(AClcStoneStall* S) {
	if (!S||!EnergyBallClass) return;
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform T = S->GetBallSpawnLocation();
	if (AClcEnergyBall* B = GetWorld()->SpawnActor<AClcEnergyBall>(EnergyBallClass,T.GetLocation(),FRotator::ZeroRotator,P)) {
		B->SetScaleValue(MapScaleToValue(CalculateStallValue(S))); EnergyBalls.Add(S,B);
	}
}
void UClcEagleEyeComponent::DestroyBall(AClcStoneStall* S) { if (AClcEnergyBall** B=EnergyBalls.Find(S)) { if (*B) (*B)->Destroy(); EnergyBalls.Remove(S); } }
void UClcEagleEyeComponent::DestroyAllBalls() { for (auto& P : EnergyBalls) if (P.Value) P.Value->Destroy(); EnergyBalls.Empty(); }
float UClcEagleEyeComponent::CalculateStallValue(AClcStoneStall* S) const { if (!S) return 0; float T=0; for (AClcStone* St : S->GetDisplayedStones()) if (St) T+=St->GetStoneData().Internal.TheoreticalValue; return T; }
float UClcEagleEyeComponent::MapScaleToValue(float V) const { if (!Config||Config->BallReferenceValue<=0) return 1; return FMath::Clamp(FMath::Lerp(Config->BallMinScale,Config->BallMaxScale,V/Config->BallReferenceValue),Config->BallMinScale,Config->BallMaxScale); }
void UClcEagleEyeComponent::OnStallRegistered(AClcStoneStall* S) { if (bActive) SpawnBall(S); }
void UClcEagleEyeComponent::OnStallUnregistered(AClcStoneStall* S) { DestroyBall(S); }