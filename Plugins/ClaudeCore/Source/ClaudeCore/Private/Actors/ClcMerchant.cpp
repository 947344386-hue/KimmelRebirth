// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcMerchant.h"
#include "ClcLog.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Data/ClcMerchantConfig.h"
#include "Data/ClcMerchantAnimConfig.h"
#include "Data/ClcMerchantBubbleConfig.h"
#include "Data/ClcMerchantTalkConfig.h"
#include "Data/ClcMerchantPersonality.h"
#include "UI/ClcMerchantBubbleWidget.h"
#include "ClcDeveloperSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
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
	DestroyBubbleWidget();
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

	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);

	const FVector TraceEnd = CameraLoc + CameraRot.Vector() * 5000.0f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	if (APawn* Pawn = PC->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);
	}

	AClcStone* HitStone = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, TraceEnd, ECC_Visibility, Params))
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

	// 瞄准变化更新嘴上话术状态——瞄准块=Aim，没瞄准=Enter（回整摊推销）
	CurrentTalkState = NewStone ? ETalkState::Aim : ETalkState::Enter;
	RefreshBubble();

	if (NewStone)
	{
		PlayMicroReactionForStone(NewStone);
	}
	else
	{
		bInMicroReaction = false;
		ReactionTimer = 0.0f;
		PlayMoodAnim();
	}
}

// ---- Tick ----

void AClcMerchant::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 气泡位置由 Actor Tick 驱动：Widget 离屏后 Slate 可能停止 NativeTick，不能让 Widget 自救。
	if (BubbleWidget)
	{
		BubbleWidget->UpdateScreenPosition();
	}

#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		DebugBubbleLogTimer += DeltaTime;
		if (DebugBubbleLogTimer >= 1.5f)
		{
			DebugBubbleLogTimer = 0.f;
			const bool bInViewport = BubbleWidget && BubbleWidget->IsInViewport();
			const int32 Vis = BubbleWidget ? (int32)BubbleWidget->GetVisibility() : -1;
			UE_LOG(LogClaudeCore, Warning,
				TEXT("[Bubble:%s] InRange=%d Widget=%d InViewport=%d Vis=%d EagleEye=%d"),
				*GetName(), PlayerInRange.IsValid()?1:0, BubbleWidget?1:0, bInViewport?1:0, Vis, bEagleEyeActive?1:0);
		}
	}
#endif

	if (!Config) return;

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

// ---- 气泡（单实例切模式）----

void AClcMerchant::ShowBubble()
{
	// 鹰眼激活——切两行模式（性格 tag + 心理话）
	bEagleEyeActive = true;
	RefreshBubble();
}

void AClcMerchant::HideBubble()
{
	// 鹰眼结束——若仍 InRange 回嘴上模式，否则隐藏
	bEagleEyeActive = false;
	RefreshBubble();
}

void AClcMerchant::RefreshBubble()
{
	const bool bShouldShow = bEagleEyeActive || PlayerInRange.IsValid();
#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] Refresh: bShouldShow=%d bEagleEye=%d InRange=%d CurWidget=%d TalkState=%d"),
			bShouldShow?1:0, bEagleEyeActive?1:0, PlayerInRange.IsValid()?1:0, BubbleWidget?1:0, (int32)CurrentTalkState);
	}
#endif
	if (!bShouldShow)
	{
		DestroyBubbleWidget();
		return;
	}

	EnsureBubbleWidget();
	if (!BubbleWidget) return;

	if (bEagleEyeActive)
	{
		// 鹰眼两行：主行心理话(诚实) + 次行性格 tag
		const FText Psyche = BubbleConfig ? BubbleConfig->PickLine(CurrentTier, LastOutcome) : FText::GetEmpty();
		BubbleWidget->SetBubbleText(Psyche);
		BubbleWidget->SetSecondaryText(Personality ? Personality->TagText : FText::GetEmpty());
	}
	else
	{
		// 嘴上模式：主行话术(可骗，按声称档位) + 次行空
		const EClcStallTier Claimed = ComputeClaimedTier();
		const FText Talk = TalkConfig ? TalkConfig->PickLine(Personality, CurrentTalkState, Claimed) : FText::GetEmpty();
		BubbleWidget->SetBubbleText(Talk);

		FText Secondary = FText::GetEmpty();
		if (CurrentTalkState == ETalkState::Aim && CurrentAimedStone.IsValid())
		{
			const FString& Pitch = CurrentAimedStone->GetStoneData().Internal.ClaimedPitch;
			if (!Pitch.IsEmpty())
			{
				Secondary = FText::FromString(Pitch);
			}
		}
		BubbleWidget->SetSecondaryText(Secondary);
	}
}

void AClcMerchant::EnsureBubbleWidget()
{
#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] Ensure: alreadyHas=%d hasClass=%d"),
			BubbleWidget?1:0, (Config && Config->BubbleWidgetClass)?1:0);
	}
#endif
	if (BubbleWidget || !Config || !Config->BubbleWidgetClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	BubbleWidget = CreateWidget<UClcMerchantBubbleWidget>(PC, Config->BubbleWidgetClass);
	if (BubbleWidget)
	{
		BubbleWidget->SetAnchor(this, Config->BubbleAnchorOffset);
		BubbleWidget->SetScreenOffset(Config->BubbleScreenOffset);
		BubbleWidget->AddToViewport(50);
		BubbleWidget->UpdateScreenPosition();
		SetActorTickInterval(0.0f); // 气泡存在期间每帧驱动屏幕投影
#if !UE_BUILD_SHIPPING
		if (GetDebugBubbleCVar()->GetInt() != 0)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] Ensure: CREATED widget=%p"), BubbleWidget);
		}
#endif
	}
}

void AClcMerchant::DestroyBubbleWidget()
{
#if !UE_BUILD_SHIPPING
	if (GetDebugBubbleCVar()->GetInt() != 0 && BubbleWidget)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Bubble] Destroy: removing widget=%p"), BubbleWidget);
	}
#endif
	if (BubbleWidget)
	{
		BubbleWidget->RemoveFromParent();
		BubbleWidget = nullptr;
		SetActorTickInterval(0.1f); // 无气泡时恢复低频 Tick
	}
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
			RefreshBubble();
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
		// 非鹰眼则隐藏；鹰眼则 RefreshBubble 保持鹰眼模式
		RefreshBubble();
	}
}

// ---- 购买反馈 ----

void AClcMerchant::OnStoneRemoved(EClcPurchaseOutcome Outcome)
{
	LastOutcome = Outcome;
	CurrentTalkState = ETalkState::Purchase;
	RecomputeTier();
	// 刷新当前模式反馈：鹰眼更新心理话，嘴上更新购入话术
	RefreshBubble();
}
