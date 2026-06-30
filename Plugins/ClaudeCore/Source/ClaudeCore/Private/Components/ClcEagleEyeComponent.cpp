// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcEagleEyeComponent.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcEnergyBall.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

UClcEagleEyeComponent::UClcEagleEyeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UClcEagleEyeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeConfig();

	MarketSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>();

	EnergyBallClass = LoadClass<AClcEnergyBall>(nullptr, TEXT("/Game/JadeBetting/Blueprints/BP_EnergyBall.BP_EnergyBall_C"));
}

void UClcEagleEyeComponent::InitializeConfig()
{
	Config = LoadObject<UClcEagleEyeConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_EagleEyeConfig"));
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcEagleEye] Failed to load DA_EagleEyeConfig! Create it at /Game/JadeBetting/Data/"));
	}
}

void UClcEagleEyeComponent::ActivateEagleEye()
{
	if (bCoolingDown)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ClcEagleEye] On cooldown, can't activate."));
		return;
	}

	if (!Config) return;

	bActive = true;
	ActiveTimer = Config->ActiveDuration;
	bCoolingDown = false;
	ScanTimer = 0.0f;

	UpdateBalls();

	UE_LOG(LogTemp, Log, TEXT("[ClcEagleEye] Activated! Duration: %.1fs"), ActiveTimer);
}

void UClcEagleEyeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Config) return;

	if (bActive)
	{
		ActiveTimer -= DeltaTime;
		ScanTimer -= DeltaTime;

		if (ScanTimer <= 0.0f)
		{
			ScanTimer = SCAN_INTERVAL;
			UpdateBalls();
		}

		if (ActiveTimer <= 0.0f)
		{
			// 激活结束 → 进入冷却
			bActive = false;
			bCoolingDown = true;
			CooldownTimer = Config->CooldownDuration;
			DestroyAllBalls();
			UE_LOG(LogTemp, Log, TEXT("[ClcEagleEye] Deactivated. Cooldown: %.1fs"), CooldownTimer);
		}
	}

	if (bCoolingDown)
	{
		CooldownTimer -= DeltaTime;
		if (CooldownTimer <= 0.0f)
		{
			bCoolingDown = false;
			UE_LOG(LogTemp, Log, TEXT("[ClcEagleEye] Cooldown finished."));
		}
	}
}

void UClcEagleEyeComponent::UpdateBalls()
{
	if (!MarketSubsystem || !EnergyBallClass) return;

	for (const auto& StallPtr : MarketSubsystem->GetStalls())
	{
		AClcStoneStall* Stall = StallPtr.Get();
		if (!Stall) continue;

		const float Value = CalculateStallValue(Stall);

		if (AClcEnergyBall** Existing = EnergyBalls.Find(Stall))
		{
			if (*Existing)
			{
				(*Existing)->SetScaleValue(MapScaleToValue(Value));
			}
		}
		else
		{
			SpawnBall(Stall);
		}
	}
}

void UClcEagleEyeComponent::SpawnBall(AClcStoneStall* Stall)
{
	if (!Stall || !EnergyBallClass) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FTransform BallTransform = Stall->GetBallSpawnLocation();
	AClcEnergyBall* Ball = GetWorld()->SpawnActor<AClcEnergyBall>(
		EnergyBallClass, BallTransform.GetLocation(), FRotator::ZeroRotator, SpawnParams);

	if (Ball)
	{
		const float Value = CalculateStallValue(Stall);
		Ball->SetScaleValue(MapScaleToValue(Value));
		EnergyBalls.Add(Stall, Ball);
	}
}

void UClcEagleEyeComponent::DestroyBall(AClcStoneStall* Stall)
{
	if (AClcEnergyBall** Ball = EnergyBalls.Find(Stall))
	{
		if (*Ball) (*Ball)->Destroy();
		EnergyBalls.Remove(Stall);
	}
}

void UClcEagleEyeComponent::DestroyAllBalls()
{
	for (auto& Pair : EnergyBalls)
	{
		if (Pair.Value) Pair.Value->Destroy();
	}
	EnergyBalls.Empty();
}

float UClcEagleEyeComponent::CalculateStallValue(AClcStoneStall* Stall) const
{
	if (!Stall) return 0.0f;

	// Σ(S_green × C_sw) per stone on the stall
	float TotalValue = 0.0f;
	for (AClcStone* Stone : Stall->GetDisplayedStones())
	{
		if (!Stone) continue;
		TotalValue += Stone->GetStoneData().Internal.TheoreticalValue;
	}
	return TotalValue;
}

float UClcEagleEyeComponent::MapScaleToValue(float Value) const
{
	if (!Config || Config->BallReferenceValue <= 0.0f) return 1.0f;

	const float Ratio = Value / Config->BallReferenceValue;
	return FMath::Clamp(FMath::Lerp(Config->BallMinScale, Config->BallMaxScale, Ratio),
		Config->BallMinScale, Config->BallMaxScale);
}

// 摊位注册/注销回调
void UClcEagleEyeComponent::OnStallRegistered(AClcStoneStall* Stall)
{
	if (bActive)
	{
		SpawnBall(Stall);
	}
}

void UClcEagleEyeComponent::OnStallUnregistered(AClcStoneStall* Stall)
{
	DestroyBall(Stall);
}
