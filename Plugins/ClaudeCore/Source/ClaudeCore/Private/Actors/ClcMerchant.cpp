// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcMerchant.h"
#include "ClcLog.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Components/ClcInteractionComponent.h"
#include "Data/ClcMerchantConfig.h"
#include "Data/ClcMerchantAnimConfig.h"
#include "Data/ClcMerchantBubbleConfig.h"
#include "Data/ClcMerchantTalkConfig.h"
#include "Data/ClcMerchantPersonality.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStallConfig.h"
#include "Engine/GameInstance.h"
#include "UI/ClcMerchantBubbleWidget.h"
#include "UI/ClcMerchantEagleEyeWidget.h"
#include "ClcDeveloperSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

// 运行时控制台开关：~ ClcMerchant.DebugDrawTalkTrigger 1  （对所有商人实例生效）
static IConsoleVariable* GetDebugDrawTalkTriggerCVar()
{
	static IConsoleVariable* CVar = []{
		return IConsoleManager::Get().RegisterConsoleVariable(
			TEXT("ClcMerchant.DebugDrawTalkTrigger"), 0,
			TEXT("Draw TalkTrigger sphere + merchant/stone center points + on-screen range status for all merchants."));
	}();
	return CVar;
}

// 运行时控制台开关：~ ClcMerchant.DebugBubble 1  （输出气泡调用链 log + 气泡 widget 屏幕状态）
static IConsoleVariable* GetDebugBubbleCVar()
{
	static IConsoleVariable* CVar = []{
		return IConsoleManager::Get().RegisterConsoleVariable(
			TEXT("ClcMerchant.DebugBubble"), 0,
			TEXT("Log RefreshBubble/Ensure/Destroy/AimChanged calls + on-screen bubble widget state."));
	}();
	return CVar;
}

AClcMerchant::AClcMerchant()
{
	PrimaryActorTick.bCanEverTick = true;
	// 平时维持原 0.1s Tick；气泡创建时临时切到每帧，确保离屏后仍能由 Actor 驱动恢复。
	PrimaryActorTick.TickInterval = 0.1f;

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 嘴上话术范围触发器——只响应本地 Pawn Overlap（参考 AClcStoneVendor 范例）
	TalkTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("TalkTrigger"));
	TalkTrigger->SetupAttachment(RootComponent);
	TalkTrigger->InitSphereRadius(400.0f);
	TalkTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TalkTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	TalkTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	TalkTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TalkTrigger->SetGenerateOverlapEvents(true);
}

void AClcMerchant::BeginPlay()
{
	Super::BeginPlay();
}

void AClcMerchant::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 解绑摊位 delegate，防止商人销毁后摊位广播到悬空对象
	if (BoundStall.IsValid())
	{
		BoundStall->OnStoneRemoved.RemoveDynamic(this, &AClcMerchant::OnStoneRemoved);
	}
	DestroyTalkBubbleWidget();
	DestroyEagleEyeWidget();
	Super::EndPlay(EndPlayReason);
}

// ---- 初始化 ----

void AClcMerchant::Initialize(AClcStoneStall* Stall)
{
	BoundStall = Stall;
	LoadConfigs();

	if (!BoundStall.IsValid()) return;

	// roll 性格——空池则无性格（不撒谎/不藏动作）
	if (Config && Config->PersonalityPool.Num() > 0)
	{
		const int32 Idx = FMath::RandRange(0, Config->PersonalityPool.Num() - 1);
		Personality = Config->PersonalityPool[Idx];
	}

	// 同步嘴上话术触发器半径
	if (Config && TalkTrigger)
	{
		TalkTrigger->SetSphereRadius(Config->TalkTriggerRadius);
	}

	// 绑 TriggerSphere overlap——走近/离开触发嘴上气泡
	if (TalkTrigger)
	{
		TalkTrigger->OnComponentBeginOverlap.AddDynamic(this, &AClcMerchant::OnTalkTriggerBeginOverlap);
		TalkTrigger->OnComponentEndOverlap.AddDynamic(this, &AClcMerchant::OnTalkTriggerEndOverlap);
	}

	// 绑定摊位石头移除委托——购买时摊位广播购买结果，商人更新气泡
	BoundStall->OnStoneRemoved.AddDynamic(this, &AClcMerchant::OnStoneRemoved);

	// 朝向补正——箭头朝向=商人面朝方向，骨骼自身面朝可能差 yaw，按配置补
	if (Config && !FMath::IsNearlyZero(Config->MeshFacingYawOffset))
	{
		FRotator R = GetActorRotation();
		R.Yaw += Config->MeshFacingYawOffset;
		SetActorRotation(R);
	}

	// 贴地——spawn 位置由摊位箭头给，Z 落到地面
	SnapToGround();

	// 把 TalkTrigger 中心对齐摊位石头中心——模拟玩家走到石头范围内时商人才开口，
	// 而非走到商人站位附近。商人偏台时此偏移补偿，使触发范围始终覆盖石头。
	// 用 SetWorldLocation 而非 SetRelativeLocation：TalkTrigger 挂在 Mesh 下，局部坐标
	// 会被 Actor 旋转二次变换，商人随摊位旋转时球心会被拽偏。SetWorldLocation 由引擎换算局部坐标。
	if (TalkTrigger && BoundStall.IsValid())
	{
		const FVector StoneCenter = BoundStall->GetStoneSpawnCenterLocation();
		TalkTrigger->SetWorldLocation(StoneCenter);
	}

	// 初始档位 + mood 动画
	RecomputeTier();
	PlayMoodAnim();
}

void AClcMerchant::LoadConfigs()
{
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (!DS) return;

	Config = LoadObject<UClcMerchantConfig>(nullptr, *DS->MerchantConfigPath);
	if (!Config)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMerchant] Failed to load MerchantConfig: %s"), *DS->MerchantConfigPath);
		return;
	}

	AnimConfig = Config->AnimConfig;
	if (!AnimConfig)
	{
		AnimConfig = LoadObject<UClcMerchantAnimConfig>(nullptr, *DS->MerchantAnimConfigPath);
	}

	BubbleConfig = Config->BubbleConfig;
	if (!BubbleConfig)
	{
		BubbleConfig = LoadObject<UClcMerchantBubbleConfig>(nullptr, *DS->MerchantBubbleConfigPath);
	}

	TalkConfig = Config->TalkConfig;
	if (!TalkConfig)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMerchant] TalkConfig missing in MerchantConfig——嘴上话术无池，气泡不显示话术"));
	}

	// 从骨骼网格体池随机抽一条——空池则不设（BP 子类指定）
	if (Config->MerchantMeshPool.Num() > 0 && Mesh)
	{
		const int32 Idx = FMath::RandRange(0, Config->MerchantMeshPool.Num() - 1);
		USkeletalMesh* Picked = Config->MerchantMeshPool[Idx];
		if (Picked)
		{
			Mesh->SetSkeletalMesh(Picked);
		}
	}
	// 确保 AnimInstance 存在——dynamic montage 需要 anim instance 才能播
	if (Mesh)
	{
		if (Config->AnimInstanceClass)
		{
			Mesh->SetAnimInstanceClass(Config->AnimInstanceClass);
		}
		else if (!Mesh->GetAnimInstance())
		{
			Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());
		}
	}
}

// ---- 贴地 ----

void AClcMerchant::SnapToGround()
{
	if (!Config) return;

	const FVector Cur = GetActorLocation();
	const FVector Start = Cur + FVector(0.0f, 0.0f, 50.0f);
	const FVector End = Cur - FVector(0.0f, 0.0f, Config->GroundSnapTraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (UWorld* World = GetWorld())
	{
		World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
		if (Hit.bBlockingHit)
		{
			SetActorLocation(FVector(Cur.X, Cur.Y, Hit.Location.Z));
		}
	}
}

// ---- 档位判定 ----

void AClcMerchant::RecomputeTier()
{
	if (!BoundStall.IsValid() || !Config) return;

	const float TotalValue = BoundStall->GetTotalTheoreticalValue();

	EClcStallTier NewTier;
	if (TotalValue >= Config->GoodTierThreshold)
	{
		NewTier = EClcStallTier::Good;
	}
	else if (TotalValue <= Config->BadTierThreshold)
	{
		NewTier = EClcStallTier::Bad;
	}
	else
	{
		NewTier = EClcStallTier::Mid;
	}

	if (NewTier != CurrentTier)
	{
		CurrentTier = NewTier;
		bClaimedTierValid = false; // 档位变，嘴上声称档位需重新决定撒谎
		if (!bInMicroReaction)
		{
			PlayMoodAnim();
		}
	}
}

// ---- 动画 ----

void AClcMerchant::PlayAnimWithBlend(UAnimSequence* Anim, bool bLoop)
{
	if (!Anim || !Mesh) return;

	UAnimInstance* AnimInst = Mesh->GetAnimInstance();
	// 仅当配置了真实 AnimBP（带 DefaultSlot）才走 dynamic montage 融合；否则用 PlayAnimation 兜底（能播但无 blend）
	// 用 Config->AnimInstanceClass 判断而非运行时 instance 类——避免 instance 未就绪时序问题
	const bool bHasRealAnimBP = (Config && Config->AnimInstanceClass != nullptr && AnimInst);

	if (bHasRealAnimBP)
	{
		const float BlendTime = (Config && Config->AnimBlendTime > 0.f) ? Config->AnimBlendTime : 0.f;
		const int32 Loops = bLoop ? 999 : 1;
		AnimInst->PlaySlotAnimationAsDynamicMontage(Anim, TEXT("DefaultSlot"), BlendTime, BlendTime, 1.f, Loops);
	}
	else
	{
		// 无真实 AnimBP——直接播序列，不依赖 slot。缺点：无 blend 融合。
		Mesh->PlayAnimation(Anim, bLoop);
	}
}

void AClcMerchant::PlayMoodAnim()
{
	if (!AnimConfig || !Mesh) return;

	UAnimSequence* Anim = nullptr;

	// 烂摊 + Nervous 池非空 → 心虚；否则全档位用自信池
	if (CurrentTier == EClcStallTier::Bad)
	{
		Anim = AnimConfig->PickNervousMood();
	}
	if (!Anim)
	{
		Anim = AnimConfig->PickConfidentMood();
	}
	if (!Anim)
	{
		Anim = AnimConfig->PickIdle();
	}

	if (Anim)
	{
		PlayAnimWithBlend(Anim, true);
	}

	if (Config)
	{
		MoodReshuffleTimer = Config->MoodReshuffleInterval;
	}
}

void AClcMerchant::PlayMicroReactionForStone(AClcStone* Stone)
{
	if (!AnimConfig || !Mesh || !Stone) return;

	// 演技 gate——演技高则藏住真实反应（不播微反应，保持 mood），少泄漏诚实信号
	if (Personality && FMath::FRand() < Personality->ActingSkill)
	{
		return;
	}

	const float StoneValue = Stone->GetStoneData().Internal.TheoreticalValue;

	// 算摊位平均价值（含当前这块）——微反应是相对摊位的"哪块更好/更烂"
	float TotalValue = 0.0f;
	int32 Count = 0;
	if (BoundStall.IsValid())
	{
		for (AClcStone* S : BoundStall->GetDisplayedStones())
		{
			if (IsValid(S))
			{
				TotalValue += S->GetStoneData().Internal.TheoreticalValue;
				Count++;
			}
		}
	}
	const float AvgValue = (Count > 0) ? (TotalValue / static_cast<float>(Count)) : StoneValue;

	// 分类：高于均值 → 贪/不舍；低于均值 → 急切脱手；接近均值 → 中性
	constexpr float Margin = 0.15f;
	UAnimSequence* Anim = nullptr;
	if (StoneValue > AvgValue * (1.0f + Margin))
	{
		Anim = AnimConfig->PickGreedyReaction();
	}
	else if (StoneValue < AvgValue * (1.0f - Margin))
	{
		Anim = AnimConfig->PickEagerReaction();
	}
	else
	{
		Anim = AnimConfig->PickNeutralReaction();
	}

	if (Anim)
	{
		PlayAnimWithBlend(Anim, false);
	}

	bInMicroReaction = true;
	ReactionTimer = Config ? Config->MicroReactionDuration : 2.5f;
}

// ---- 瞄准检测 ----

void AClcMerchant::TickAimedStone()
{
	if (!BoundStall.IsValid()) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;
	APawn* Pawn = PC->GetPawn();

	// 收敛路径：角色挂了 UClcInteractionComponent 时，读其唯一中心球扫结果，
	// 不再自做 5000 球扫——单 trace/单容差，消除旧"Indicator 选中但商人细射线擦边没中→气泡偶尔没种水"的分叉。
	// 中心组件 LookDistance(默认 6000) ≥ 旧 5000，商人长距离瞄准反应不缩短。
	if (UClcInteractionComponent* InterComp = Pawn ? Pawn->FindComponentByClass<UClcInteractionComponent>() : nullptr)
	{
		AActor* LookedAt = InterComp->GetLookedAtActor();
		AClcStone* HitStone = nullptr;
		if (LookedAt && LookedAt->IsA(AClcStone::StaticClass()))
		{
			for (AClcStone* S : BoundStall->GetDisplayedStones())
			{
				if (S == LookedAt)
				{
					HitStone = S;
					break;
				}
			}
		}
		OnAimedStoneChanged(HitStone);
		return;
	}

	// 回退：角色未挂中心组件时走旧自检（与收敛前等价，不引入新分叉；加回组件即自动收敛）。
	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);

	const FVector TraceEnd = CameraLoc + CameraRot.Vector() * 5000.0f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (Pawn)
	{
		Params.AddIgnoredActor(Pawn);
	}

	float AimSweepR = 25.0f;
	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UClcStoneMarketSubsystem* Market = GI->GetSubsystem<UClcStoneMarketSubsystem>())
		{
			if (UClcStallConfig* StallCfg = Market->GetStallConfig())
			{
				AimSweepR = FMath::Max(0.0f, StallCfg->StoneAimSweepRadius);
			}
		}
	}

	AClcStone* HitStone = nullptr;
	const bool bHit = (AimSweepR > SMALL_NUMBER)
		? GetWorld()->SweepSingleByChannel(Hit, CameraLoc, TraceEnd, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(AimSweepR), Params)
		: GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_Visibility, Params);
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->IsA(AClcStone::StaticClass()))
		{
			for (AClcStone* S : BoundStall->GetDisplayedStones())
			{
				if (S == HitActor)
				{
					HitStone = S;
					break;
				}
			}
		}
	}

	OnAimedStoneChanged(HitStone);
}

void AClcMerchant::OnAimedStoneChanged(AClcStone* NewStone)
{
	if (NewStone && !IsValid(NewStone)) return;
	if (CurrentAimedStone.Get() == NewStone) return;

#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] AimChanged (changed): new=%s"),
			NewStone?*NewStone->GetName():TEXT("none"));
	}
#endif
	CurrentAimedStone = NewStone;

	// 购买反馈保留期间仍同步瞄准目标和动作，到期后直接恢复到最新目标的话术。
	if (PurchaseFeedbackTimer <= 0.0f)
	{
		CurrentTalkState = NewStone ? ETalkState::Aim : ETalkState::Enter;
		RefreshTalkBubble();
	}

	if (NewStone)
	{
		PlayMicroReactionForStone(NewStone);
		MicroReactionRetriggerTimer = Config ? Config->MicroReactionDuration : 2.5f;
	}
	else
	{
		bInMicroReaction = false;
		ReactionTimer = 0.0f;
		MicroReactionRetriggerTimer = 0.0f;
		PlayMoodAnim();
	}
}

// ---- Tick ----

void AClcMerchant::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// UI 位置由 Actor Tick 驱动：Widget 离屏后 Slate 可能停止 NativeTick，不能让 Widget 自救。
	if (TalkBubbleWidget)
	{
		TalkBubbleWidget->UpdateScreenPosition();
	}

	if (EagleEyeWidget)
	{
		EagleEyeWidget->UpdateScreenPosition();
	}

#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		DebugBubbleLogTimer += DeltaTime;
		if (DebugBubbleLogTimer >= 1.5f)
		{
			DebugBubbleLogTimer = 0.f;
			const bool bTalkInViewport = TalkBubbleWidget && TalkBubbleWidget->IsInViewport();
			const bool bEagleInViewport = EagleEyeWidget && EagleEyeWidget->IsInViewport();
			UE_LOG(LogClaudeCore, Warning,
				TEXT("[MerchantUI:%s] InRange=%d Talk=%d TalkViewport=%d Eagle=%d EagleViewport=%d EagleEye=%d"),
				*GetName(), PlayerInRange.IsValid()?1:0, TalkBubbleWidget?1:0, bTalkInViewport?1:0,
				EagleEyeWidget?1:0, bEagleInViewport?1:0, bEagleEyeActive?1:0);
		}
	}
#endif

	if (!Config) return;

	// 鹰眼残留倒计时——per-merchant 独立；到点销毁洞察 UI（范围外商人继续走自己的残留）。
	if (bEagleEyeActive)
	{
		EagleEyeTimer -= DeltaTime;
		if (EagleEyeTimer <= 0.0f)
		{
			bEagleEyeActive = false;
			RefreshEagleEyeWidget();
		}
	}

	if (PurchaseFeedbackTimer > 0.0f)
	{
		PurchaseFeedbackTimer -= DeltaTime;
		if (PurchaseFeedbackTimer <= 0.0f)
		{
			PurchaseFeedbackTimer = 0.0f;
			CurrentTalkState = CurrentAimedStone.IsValid() ? ETalkState::Aim : ETalkState::Enter;
			RefreshTalkBubble();
		}
	}

	// 微反应倒计时
	if (bInMicroReaction)
	{
		ReactionTimer -= DeltaTime;
		if (ReactionTimer <= 0.0f)
		{
			bInMicroReaction = false;
			PlayMoodAnim();
		}
	}

	// mood 重抽
	if (!bInMicroReaction && Config->MoodReshuffleInterval > 0.0f)
	{
		MoodReshuffleTimer -= DeltaTime;
		if (MoodReshuffleTimer <= 0.0f)
		{
			PlayMoodAnim();
		}
	}

	// Aim 态持续时节律重播微反应——球扫后瞄准稳定，不再靠抖动触发 OnAimedStoneChanged，
	// 需主动节律，否则玩家持续瞄一块石头时商人无动作反馈。每次仍过 ActingSkill gate。
	if (CurrentTalkState == ETalkState::Aim && CurrentAimedStone.IsValid() && !bInMicroReaction)
	{
		MicroReactionRetriggerTimer -= DeltaTime;
		if (MicroReactionRetriggerTimer <= 0.0f)
		{
			MicroReactionRetriggerTimer = Config->MicroReactionDuration;
			PlayMicroReactionForStone(CurrentAimedStone.Get());
		}
	}

	// 瞄准检测维持原 0.1 秒节奏，避免 Actor 改为每帧 Tick 后增加 trace 开销。
	AimTraceTimer -= DeltaTime;
	if (AimTraceTimer <= 0.0f)
	{
		AimTraceTimer = 0.1f;
		TickAimedStone();
	}

#if !UE_BUILD_SHIPPING
	if (GetDebugDrawTalkTriggerCVar()->GetInt() != 0 && TalkTrigger && GetWorld())
	{
		const bool bInRange = PlayerInRange.IsValid();
		const FVector TriggerCenter = TalkTrigger->GetComponentLocation();
		const float Radius = TalkTrigger->GetScaledSphereRadius();

		// 触发球：玩家在内=绿，在外=黄
		DrawDebugSphere(GetWorld(), TriggerCenter, Radius, 24,
			bInRange ? FColor::Green : FColor::Yellow, false, 0.15f, 0, 2.0f);

		// 商人位置（青）→ 石头中心（品红）连线——确认触发中心是否已对齐石头
		const FVector MyLoc = GetActorLocation();
		DrawDebugPoint(GetWorld(), MyLoc, 15.f, FColor::Cyan, false, 0.15f);
		if (BoundStall.IsValid())
		{
			const FVector StoneCenter = BoundStall->GetStoneSpawnCenterLocation();
			DrawDebugPoint(GetWorld(), StoneCenter, 15.f, FColor::Magenta, false, 0.15f);
			DrawDebugLine(GetWorld(), MyLoc, StoneCenter, FColor::White, false, 0.15f, 0, 1.5f);
		}

		// 玩家位置（绿点）+ 屏幕状态
		if (bInRange && PlayerInRange.IsValid())
		{
			DrawDebugPoint(GetWorld(), PlayerInRange->GetActorLocation(), 20.f, FColor::Green, false, 0.15f);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				4321, 0.15f, bInRange ? FColor::Green : FColor::Yellow,
				FString::Printf(TEXT("[TalkTrigger] %s  Center=(%.0f,%.0f,%.0f)  R=%.0f  bEagleEye=%d"),
					bInRange ? TEXT("IN RANGE ✓") : TEXT("OUT ✗"),
					TriggerCenter.X, TriggerCenter.Y, TriggerCenter.Z, Radius,
					bEagleEyeActive ? 1 : 0));
		}
	}
#endif
}

// ---- 性格驱动：声称档位 ----

EClcStallTier AClcMerchant::ComputeClaimedTier()
{
	// 商人一旦决定演某档就稳定（避免每帧重 roll 话术跳变）；档位变化时 RecomputeTier 失效缓存
	if (bClaimedTierValid)
	{
		return CachedClaimedTier;
	}

	// 好摊不撒谎（已经好）；非好摊按撒谎倾向概率被说成好摊
	if (CurrentTier == EClcStallTier::Good)
	{
		CachedClaimedTier = EClcStallTier::Good;
	}
	else if (Personality && FMath::FRand() < Personality->DeceptionLevel)
	{
		CachedClaimedTier = EClcStallTier::Good;
	}
	else
	{
		CachedClaimedTier = CurrentTier;
	}
	bClaimedTierValid = true;
	return CachedClaimedTier;
}

float AClcMerchant::GetDeceptionLevel() const
{
	return Personality ? Personality->DeceptionLevel : 0.5f;
}

// ---- 独立口头气泡 / 鹰眼洞察 ----

void AClcMerchant::ShowBubble(float Duration)
{
	bEagleEyeActive = true;
	EagleEyeTimer = Duration;
	RefreshEagleEyeWidget();
}

void AClcMerchant::HideBubble()
{
	bEagleEyeActive = false;
	EagleEyeTimer = 0.0f;
	RefreshEagleEyeWidget();
}

void AClcMerchant::RefreshTalkBubble()
{
#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[TalkBubble] Refresh: InRange=%d Widget=%d TalkState=%d"),
			PlayerInRange.IsValid()?1:0, TalkBubbleWidget?1:0, (int32)CurrentTalkState);
	}
#endif
	if (!PlayerInRange.IsValid())
	{
		DestroyTalkBubbleWidget();
		return;
	}

	EnsureTalkBubbleWidget();
	if (!TalkBubbleWidget) return;

	const EClcStallTier Claimed = ComputeClaimedTier();
	const FText Talk = TalkConfig ? TalkConfig->PickLine(Personality, CurrentTalkState, Claimed) : FText::GetEmpty();
	TalkBubbleWidget->SetBubbleText(Talk);

	FText Secondary = FText::GetEmpty();
	if (CurrentTalkState == ETalkState::Aim && CurrentAimedStone.IsValid())
	{
		const FString& Pitch = CurrentAimedStone->GetStoneData().Internal.ClaimedPitch;
		if (!Pitch.IsEmpty())
		{
			Secondary = FText::FromString(Pitch);
		}
	}
	TalkBubbleWidget->SetSecondaryText(Secondary);
}

void AClcMerchant::RefreshEagleEyeWidget()
{
#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[EagleEyeWidget] Refresh: Active=%d Widget=%d"),
			bEagleEyeActive?1:0, EagleEyeWidget?1:0);
	}
#endif
	if (!bEagleEyeActive)
	{
		DestroyEagleEyeWidget();
		return;
	}

	EnsureEagleEyeWidget();
	if (!EagleEyeWidget) return;

	const FText Psyche = BubbleConfig ? BubbleConfig->PickLine(CurrentTier, LastOutcome) : FText::GetEmpty();

	// 主行：性格 tag + 邪恶度配色（DeceptionLevel 高=越骗越紫，低=偏青）
	const FString TagStr = Personality ? Personality->TagText.ToString() : TEXT("神秘人");
	EagleEyeWidget->SetBubbleText(FText::FromString(FString::Printf(TEXT("性格：%s"), *TagStr)));

	const FLinearColor GoodColor(0.05f, 0.80f, 0.70f);  // 青（善良）
	const FLinearColor EvilColor(0.60f, 0.15f, 0.80f);  // 紫（邪恶）
	const float Evilness = Personality ? FMath::Clamp(Personality->DeceptionLevel, 0.0f, 1.0f) : 0.5f;
	EagleEyeWidget->SetPersonalityColor(FLinearColor::LerpUsingHSV(GoodColor, EvilColor, Evilness));

	// 次行：心理侧写
	EagleEyeWidget->SetSecondaryText(FText::FromString(FString::Printf(TEXT("心理侧写：%s"), *Psyche.ToString())));
}

void AClcMerchant::EnsureTalkBubbleWidget()
{
	if (TalkBubbleWidget || !Config || !Config->TalkBubbleWidgetClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	TalkBubbleWidget = CreateWidget<UClcMerchantBubbleWidget>(PC, Config->TalkBubbleWidgetClass);
	if (TalkBubbleWidget)
	{
		TalkBubbleWidget->SetAnchor(Mesh, Config->TalkBubbleAnchorOffset);
		TalkBubbleWidget->SetSimulatedPerspective(Config->UISimulatedPerspective);
		TalkBubbleWidget->AddToViewport(50);
		TalkBubbleWidget->UpdateScreenPosition();
		UpdateWidgetTickInterval();
	}
}

void AClcMerchant::EnsureEagleEyeWidget()
{
	if (EagleEyeWidget || !Config || !Config->EagleEyeWidgetClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	EagleEyeWidget = CreateWidget<UClcMerchantEagleEyeWidget>(PC, Config->EagleEyeWidgetClass);
	if (EagleEyeWidget)
	{
		EagleEyeWidget->SetAnchor(Mesh, Config->EagleEyeAnchorOffset);
		EagleEyeWidget->SetSimulatedPerspective(Config->UISimulatedPerspective);
		EagleEyeWidget->AddToViewport(51);
		EagleEyeWidget->UpdateScreenPosition();
		UpdateWidgetTickInterval();
	}
}

void AClcMerchant::DestroyTalkBubbleWidget()
{
	if (TalkBubbleWidget)
	{
		TalkBubbleWidget->RemoveFromParent();
		TalkBubbleWidget = nullptr;
		UpdateWidgetTickInterval();
	}
}

void AClcMerchant::DestroyEagleEyeWidget()
{
	if (EagleEyeWidget)
	{
		EagleEyeWidget->RemoveFromParent();
		EagleEyeWidget = nullptr;
		UpdateWidgetTickInterval();
	}
}

void AClcMerchant::UpdateWidgetTickInterval()
{
	SetActorTickInterval((TalkBubbleWidget || EagleEyeWidget) ? 0.0f : 0.1f);
}

// ---- TriggerSphere overlap ----

void AClcMerchant::OnTalkTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APawn* Pawn = Cast<APawn>(Other))
	{
		if (Pawn->IsLocallyControlled())
		{
#if !UE_BUILD_SHIPPING
			if (GetDebugBubbleCVar()->GetInt() != 0)
			{
				UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] BeginOverlap triggered (set InRange)"));
			}
#endif
			PlayerInRange = Pawn;
			CurrentTalkState = ETalkState::Enter;
			RefreshTalkBubble();
		}
	}
}

void AClcMerchant::OnTalkTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (PlayerInRange.Get() == Other)
	{
#if !UE_BUILD_SHIPPING
		if (GetDebugBubbleCVar()->GetInt() != 0)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] EndOverlap triggered (reset InRange → will DestroyBubble)"));
		}
#endif
		PlayerInRange.Reset();
		// 离开范围只销毁口头气泡；鹰眼洞察由技能生命周期独立控制。
		RefreshTalkBubble();
	}
}

// ---- 购买反馈 ----

void AClcMerchant::OnStoneRemoved(EClcPurchaseOutcome Outcome)
{
	LastOutcome = Outcome;
	CurrentTalkState = ETalkState::Purchase;
	PurchaseFeedbackTimer = FMath::Max(Config ? Config->PurchaseFeedbackDuration : 1.8f, 0.1f);
	RecomputeTier();
	// 两个通道分别更新：口头话术仅在范围内，心理话仅在鹰眼激活时。
	RefreshTalkBubble();
	RefreshEagleEyeWidget();
}
