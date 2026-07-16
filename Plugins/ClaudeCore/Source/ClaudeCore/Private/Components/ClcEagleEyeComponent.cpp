// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcEagleEyeComponent.h"
#include "ClcLog.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "ClcDeveloperSettings.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcMerchant.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UClcEagleEyeComponent::UClcEagleEyeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UClcEagleEyeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeConfig();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			MarketSubsystem = GI->GetSubsystem<UClcStoneMarketSubsystem>();
		}
	}
}

void UClcEagleEyeComponent::InitializeConfig()
{
	Config = LoadObject<UClcEagleEyeConfig>(nullptr, *GetDefault<UClcDeveloperSettings>()->EagleEyeConfigPath);
	if (!Config)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcEagleEye] Failed to load EagleEyeConfig! Path: %s (check Project Settings → ClaudeCore)"),
			*GetDefault<UClcDeveloperSettings>()->EagleEyeConfigPath);
	}
}

void UClcEagleEyeComponent::ActivateEagleEye()
{
	if (bCoolingDown)
	{
		UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcEagleEye] On cooldown, can't activate."));
		return;
	}

	if (!Config) return;

	bActive = true;
	ActiveTimer = Config->ActiveDuration;
	bCoolingDown = false;

	ToggleMerchantBubbles(true);
}

void UClcEagleEyeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 注：不在此禁用 Tick——ActivateEagleEye 由蓝图异步触发，Tick 启停时序与按键不同步会导致
	// 间歇性"偶尔没效果"。Tick 一直运行的开销极小（几个 float 比较），保持稳定优先。
	if (!Config) return;

	if (bActive)
	{
		ActiveTimer -= DeltaTime;

		if (ActiveTimer <= 0.0f)
		{
			// 激活结束 → 关气泡 → 进入冷却
			bActive = false;
			bCoolingDown = true;
			CooldownTimer = Config->CooldownDuration;
			ToggleMerchantBubbles(false);
		}
	}

	if (bCoolingDown)
	{
		CooldownTimer -= DeltaTime;
		if (CooldownTimer <= 0.0f)
		{
			bCoolingDown = false;
		}
	}
}

void UClcEagleEyeComponent::ToggleMerchantBubbles(bool bShow)
{
	if (!MarketSubsystem) return;

	for (const auto& StallPtr : MarketSubsystem->GetStalls())
	{
		AClcStoneStall* Stall = StallPtr.Get();
		if (!Stall) continue;

		AClcMerchant* Merchant = Stall->GetMerchant();
		if (!Merchant) continue;

		if (bShow)
		{
			Merchant->ShowBubble();
		}
		else
		{
			Merchant->HideBubble();
		}
	}
}