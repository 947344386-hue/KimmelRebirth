// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcMerchant.h"
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

AClcMerchant::AClcMerchant()
{
	PrimaryActorTick.bCanEverTick = true;
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
		UE_LOG(LogTemp, Error, TEXT("[ClcMerchant] Failed to load MerchantConfig: %s"), *DS->MerchantConfigPath);
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
		UE_LOG(LogTemp, Warning, TEXT("[ClcMerchant] TalkConfig missing in MerchantConfig——嘴上话术无池，气泡不显示话术"));
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

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		SetActorLocation(FVector(Cur.X, Cur.Y, Hit.Location.Z));
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
	if (CurrentAimedStone.Get() == NewStone) return;

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

	// 瞄准检测
	TickAimedStone();
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
	const bool bShouldShow = bEagleEyeActive || bPlayerInRange;
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
		BubbleWidget->SetSecondaryText(FText::GetEmpty());
	}
}

void AClcMerchant::EnsureBubbleWidget()
{
	if (BubbleWidget || !Config || !Config->BubbleWidgetClass) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	BubbleWidget = CreateWidget<UClcMerchantBubbleWidget>(PC, Config->BubbleWidgetClass);
	if (BubbleWidget)
	{
		BubbleWidget->SetAnchor(this, Config->BubbleAnchorOffset);
		BubbleWidget->SetScreenOffset(Config->BubbleScreenOffset);
		BubbleWidget->AddToViewport(50);
	}
}

void AClcMerchant::DestroyBubbleWidget()
{
	if (BubbleWidget)
	{
		BubbleWidget->RemoveFromParent();
		BubbleWidget = nullptr;
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
			PlayerInRange = Pawn;
			bPlayerInRange = true;
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
		PlayerInRange.Reset();
		bPlayerInRange = false;
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
