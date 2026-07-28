// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcEagleEyeComponent.h"
#include "ClcLog.h"
#include "Data/ClcEagleEyeConfig.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "ClcDeveloperSettings.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcMerchant.h"
#include "Components/PostProcessComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

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

bool UClcEagleEyeComponent::InitializeScanEffect()
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Config || !World || World->GetNetMode() == NM_DedicatedServer || !Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}

	if (ScanPostProcessComponent && ScanMID)
	{
		return true;
	}

	UMaterialInterface* ScanMaterial = Config->ScanPostProcessMaterial.LoadSynchronous();
	if (!ScanMaterial)
	{
		UE_LOG(LogClaudeCore, Warning,
			TEXT("[ClcEagleEye] Scan material is not configured. Material=%s"),
			*Config->ScanPostProcessMaterial.ToString());
		CleanupScanEffect();
		return false;
	}

	if (!ScanMID)
	{
		ScanMID = UMaterialInstanceDynamic::Create(ScanMaterial, this);
		if (!ScanMID)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcEagleEye] Failed to create scan dynamic material instance."));
			CleanupScanEffect();
			return false;
		}
	}

	if (!ScanPostProcessComponent)
	{
		ScanPostProcessComponent = NewObject<UPostProcessComponent>(Pawn, NAME_None, RF_Transient);
		if (!ScanPostProcessComponent)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcEagleEye] Failed to create scan post-process component."));
			CleanupScanEffect();
			return false;
		}

		ScanPostProcessComponent->bUnbound = true;
		ScanPostProcessComponent->bEnabled = true;
		ScanPostProcessComponent->BlendWeight = 1.0f;
		ScanPostProcessComponent->Priority = 100.0f;
		ScanPostProcessComponent->AddOrUpdateBlendable(ScanMID, 1.0f);
		ScanPostProcessComponent->RegisterComponent();
	}

	return true;
}

void UClcEagleEyeComponent::StartOrRestartScanPulse(const FVector& Center)
{
	if (!Config || Config->ScanRadius <= 0.0f)
	{
		CleanupScanEffect();
		return;
	}

	if (!InitializeScanEffect())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !ScanMID)
	{
		CleanupScanEffect();
		return;
	}

	// ScanFX 扫描波靠材质内部“当前游戏时间 - Scan Start Time”自动扩散，
	// 这里只需在按键瞬间写入起点与参数，材质自行按时间扩散并在 Scan Duration 后淡出。
	static const FName ScanStartLocationName(TEXT("Scan Start Location"));
	static const FName ScanStartTimeName(TEXT("Scan Start Time"));
	static const FName ScanDurationName(TEXT("Scan Duration"));
	static const FName ScanRadiusName(TEXT("Scan Radius"));

	const float Duration = FMath::Max(0.0f, Config->ScanDuration);
	const float Radius = FMath::Max(0.0f, Config->ScanRadius);

	ScanMID->SetVectorParameterValue(ScanStartLocationName, FLinearColor(Center.X, Center.Y, Center.Z, 1.0f));
	ScanMID->SetScalarParameterValue(ScanStartTimeName, World->GetTimeSeconds());
	ScanMID->SetScalarParameterValue(ScanDurationName, Duration);
	ScanMID->SetScalarParameterValue(ScanRadiusName, Radius);

	bScanActive = true;
	// 扩散时长 + 余量，让材质淡出完成后再销毁后处理组件
	ScanEndTimer = Duration + 1.0f;
}

void UClcEagleEyeComponent::CleanupScanEffect()
{
	bScanActive = false;
	ScanEndTimer = 0.0f;

	if (ScanPostProcessComponent)
	{
		ScanPostProcessComponent->DestroyComponent();
		ScanPostProcessComponent = nullptr;
	}
	ScanMID = nullptr;
}

bool UClcEagleEyeComponent::IsInExclusiveFlow() const
{
	AActor* Owner = GetOwner();
	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn) return true;

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return true;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return true;

	if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
	{
		if (KP->IsInExclusiveFlow()) return true;
	}
	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
	{
		if (BP->IsBackpackOpen()) return true;
	}
	return false;
}

void UClcEagleEyeComponent::ActivateEagleEye()
{
	if (!Config) return;

	if (IsInExclusiveFlow()) return;

	if (bCoolingDown)
	{
		UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcEagleEye] On cooldown, can't activate."));
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	const FVector Center = Owner->GetActorLocation();
	const float RadiusSq = FMath::Square(FMath::Max(0.0f, Config->ScanRadius));

	if (MarketSubsystem)
	{
		for (const auto& StallPtr : MarketSubsystem->GetStalls())
		{
			AClcStoneStall* Stall = StallPtr.Get();
			if (!Stall) continue;

			AClcMerchant* Merchant = Stall->GetMerchant();
			if (!Merchant) continue;

			if (FVector::DistSquared(Merchant->GetActorLocation(), Center) <= RadiusSq)
			{
				Merchant->ShowBubble(FMath::Max(0.0f, Config->ActiveDuration));
			}
		}
	}

	StartOrRestartScanPulse(Center);

	CooldownTimer = FMath::Max(0.0f, Config->CooldownDuration);
	bCoolingDown = CooldownTimer > 0.0f;
}

void UClcEagleEyeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 首帧 PC 就绪后注册常驻 Q 提示（BeginPlay 时角色可能尚未被 Possess）
	if (EagleEyePromptHandle == 0)
	{
		if (AActor* Owner = GetOwner())
		{
			if (APawn* Pawn = Cast<APawn>(Owner))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					if (ULocalPlayer* LP = PC->GetLocalPlayer())
					{
						if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
						{
							EagleEyePromptHandle = KP->RegisterKeyPrompt(
								EKeys::Q,
								NSLOCTEXT("ClcEagleEye", "EagleEyePromptLabel", "鹰眼"),
								FName("EagleEye"), 50);
						}
					}
				}
			}
		}
	}

	// 注：不在此禁用 Tick——ActivateEagleEye 由蓝图异步触发，Tick 启停时序与按键不同步会导致
	// 间歇性"偶尔没效果"。Tick 一直运行的开销极小（几个 float 比较），保持稳定优先。
	if (!Config) return;

	// 仅走玩家侧 CD；激活态与残留计时已下沉到每个商人自己的 Tick。
	if (bCoolingDown)
	{
		CooldownTimer -= DeltaTime;
		if (CooldownTimer <= 0.0f)
		{
			bCoolingDown = false;
		}
	}

	// 扫描视觉销毁计时：材质自己按游戏时间扩散并淡出，这里只负责到点清理后处理组件
	if (bScanActive)
	{
		ScanEndTimer -= DeltaTime;
		if (ScanEndTimer <= 0.0f)
		{
			CleanupScanEffect();
		}
	}
}

void UClcEagleEyeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 兜底注销按键提示（Owner/PC 可能已失效，子系统 Deinitialize 会统一清理）
	if (EagleEyePromptHandle != 0)
	{
		if (AActor* Owner = GetOwner())
		{
			if (APawn* Pawn = Cast<APawn>(Owner))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					if (ULocalPlayer* LP = PC->GetLocalPlayer())
					{
						if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
						{
							KP->UnregisterKeyPrompt(EagleEyePromptHandle);
						}
					}
				}
			}
		}
		EagleEyePromptHandle = 0;
	}

	CleanupScanEffect();
	Super::EndPlay(EndPlayReason);
}
