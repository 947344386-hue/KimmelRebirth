// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcHaggleComponent.h"
#include "ClcDeveloperSettings.h"
#include "ClcLog.h"
#include "UI/ClcHaggleWidget.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UClcHaggleComponent::UClcHaggleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// 选择/QTE 都需要每帧轮询按键，Tick 间隔保持默认
}

void UClcHaggleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseWidget();
	Phase = EClcHagglePhase::Idle;
	Super::EndPlay(EndPlayReason);
}

// ============================================================
// 配置 / 工具
// ============================================================

UClcHaggleConfig* UClcHaggleComponent::LoadConfig()
{
	if (Config) return Config;

	const FString Path = GetDefault<UClcDeveloperSettings>()->HaggleConfigPath;
	if (!Path.IsEmpty())
	{
		Config = LoadObject<UClcHaggleConfig>(nullptr, *Path);
	}
	if (!Config)
	{
		// DA 缺失或加载失败 → 用 CDO（HaggleTiers 空 → ResolveTiers 用内置默认）
		Config = GetMutableDefault<UClcHaggleConfig>();
	}
	return Config;
}

UClcHaggleConfig* UClcHaggleComponent::GetHaggleConfig()
{
	return LoadConfig();
}

const TArray<FClcHaggleTier>& UClcHaggleComponent::ResolveTiers() const
{
	static const TArray<FClcHaggleTier> DefaultTiers = []()
	{
		TArray<FClcHaggleTier> T;
		auto Add = [&](float Ratio, int32 Len, const TCHAR* Label)
		{
			FClcHaggleTier& Tier = T.AddDefaulted_GetRef();
			Tier.UpliftRatio = Ratio;
			Tier.SequenceLength = Len;
			Tier.Label = FText::FromString(Label);
		};
		Add(0.10f, 4, TEXT("+10%"));
		Add(0.20f, 6, TEXT("+20%"));
		Add(0.30f, 8, TEXT("+30%"));
		Add(0.50f, 12, TEXT("+50%"));
		return T;
	}();

	if (Config && Config->HaggleTiers.Num() > 0)
	{
		return Config->HaggleTiers;
	}
	return DefaultTiers;
}

UClcStoneMarketSubsystem* UClcHaggleComponent::GetMarket() const
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			return GI->GetSubsystem<UClcStoneMarketSubsystem>();
		}
	}
	return nullptr;
}

uint8 UClcHaggleComponent::RandomKeyCode() const
{
	return static_cast<uint8>(FMath::RandRange(0, 3)); // 0=W 1=A 2=S 3=D
}

// ============================================================
// 外部入口
// ============================================================

void UClcHaggleComponent::StartHaggle(int32 InReferencePrice, APlayerController* PC)
{
	if (Phase != EClcHagglePhase::Idle || !PC) return;

	CachedPC = PC;
	ReferencePrice = FMath::Max(0, InReferencePrice);
	LoadConfig();
	OpenWidget(PC);
	BeginSelection();
	OnHaggleOpened.Broadcast();
}

void UClcHaggleComponent::RequestCancel()
{
	if (Phase != EClcHagglePhase::Selection) return; // 一锤子：Playing 阶段不可取消
	Resolve(EClcHaggleOutcome::Cancelled);
}

void UClcHaggleComponent::RequestEsc()
{
	switch (Phase)
	{
	case EClcHagglePhase::Selection:
		// 选择阶段 Esc：取消整个讨价，回查看（不锁价）
		Resolve(EClcHaggleOutcome::Cancelled);
		break;
	case EClcHagglePhase::Playing:
		// QTE 阶段 Esc：只要还没失败（按错/超时），可回退到选档/直接售出
		RetreatToSelection();
		break;
	default:
		break; // Resolved/Idle 忽略
	}
}

void UClcHaggleComponent::RetreatToSelection()
{
	Phase = EClcHagglePhase::Selection;

	// 重置所有边沿检测，避免回退瞬间误触发
	bSpacePrev = false;
	for (int32 i = 0; i < 6; ++i) bNumPrev[i] = false;
	bWPrev = bAPrev = bSPrev = bDPrev = false;
	bUpPrev = bDownPrev = bLeftPrev = bRightPrev = false;

	if (Widget)
	{
		Widget->SetupSelection(ReferencePrice, ResolveTiers(), Config);
	}
}

void UClcHaggleComponent::ChooseAccept()
{
	if (Phase != EClcHagglePhase::Selection) return;
	AppliedRatio = 0.0f;
	Resolve(EClcHaggleOutcome::Accepted);
}

void UClcHaggleComponent::ChooseTier(int32 TierIndex)
{
	if (Phase != EClcHagglePhase::Selection) return;
	BeginTierSequence(TierIndex);
}

// ============================================================
// 相位推进
// ============================================================

void UClcHaggleComponent::OpenWidget(APlayerController* PC)
{
	CloseWidget();

	TSubclassOf<UClcHaggleWidget> WidgetClass = UClcHaggleWidget::StaticClass();
	if (Config && Config->HaggleWidgetClass)
	{
		WidgetClass = Config->HaggleWidgetClass;
	}

	Widget = CreateWidget<UClcHaggleWidget>(PC, WidgetClass);
	if (Widget)
	{
		Widget->SetOwningComponent(this);
		Widget->AddToViewport(20); // 盖过 VendorHUD(10)
	}
}

void UClcHaggleComponent::CloseWidget()
{
	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
}

void UClcHaggleComponent::BeginSelection()
{
	Phase = EClcHagglePhase::Selection;

	// 重置选择阶段边沿检测
	bSpacePrev = false;
	for (int32 i = 0; i < 6; ++i) bNumPrev[i] = false;
	bWPrev = bAPrev = bSPrev = bDPrev = false;
	bUpPrev = bDownPrev = bLeftPrev = bRightPrev = false;

	if (Widget)
	{
		Widget->SetupSelection(ReferencePrice, ResolveTiers(), Config);
	}
}

void UClcHaggleComponent::BeginTierSequence(int32 TierIndex)
{
	const TArray<FClcHaggleTier>& Tiers = ResolveTiers();
	if (!Tiers.IsValidIndex(TierIndex))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Haggle] Invalid tier index: %d"), TierIndex);
		return;
	}

	AppliedRatio = Tiers[TierIndex].UpliftRatio;
	const int32 Len = FMath::Max(1, Tiers[TierIndex].SequenceLength);

	Sequence.Reset(Len);
	for (int32 i = 0; i < Len; ++i)
	{
		Sequence.Add(RandomKeyCode());
	}

	CurrentKeyIndex = 0;
	PerKeyTimer = Config->TimePerKey;
	bTimerStarted = false; // 先展示序列，等玩家按下第一个正确键再启动计时

	// 重置 WASD/方向键边沿，保证首个按键的 rising edge 能被捕获
	bWPrev = bAPrev = bSPrev = bDPrev = false;
	bUpPrev = bDownPrev = bLeftPrev = bRightPrev = false;

	Phase = EClcHagglePhase::Playing;

	if (Widget)
	{
		Widget->StartSequence(Sequence);
	}
}

void UClcHaggleComponent::Resolve(EClcHaggleOutcome Outcome)
{
	Phase = EClcHagglePhase::Resolved;
	ResolvedOutcome = Outcome;

	const bool bSuccess = (Outcome == EClcHaggleOutcome::Success);

	if (Outcome == EClcHaggleOutcome::Accepted || Outcome == EClcHaggleOutcome::Cancelled)
	{
		ResolvedFinalPrice = ReferencePrice;
	}
	else if (UClcStoneMarketSubsystem* Market = GetMarket())
	{
		ResolvedFinalPrice = Market->CalculateHagglePrice(ReferencePrice, AppliedRatio, bSuccess);
	}
	else
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[Haggle] Market unavailable, fall back to reference price"));
		ResolvedFinalPrice = ReferencePrice;
	}

	ResolveTimer = (Outcome == EClcHaggleOutcome::Cancelled)
		? 0.4f
		: FMath::Max(0.0f, Config->ResolveDelay);

	if (Widget)
	{
		FText Line;
		auto FormatWithPrice = [&](const FText& Tmpl) -> FText
		{
			TArray<FStringFormatArg> Args;
			Args.Add(FStringFormatArg(ResolvedFinalPrice));
			return FText::FromString(FString::Format(*Tmpl.ToString(), Args));
		};

		if (Outcome == EClcHaggleOutcome::Cancelled)
		{
			Line = Config->CancelLine;
		}
		else if (Outcome == EClcHaggleOutcome::Accepted)
		{
			Line = FormatWithPrice(Config->AcceptLineTemplate);
		}
		else if (bSuccess)
		{
			Line = FormatWithPrice(Config->SuccessLineTemplate);
		}
		else
		{
			Line = FormatWithPrice(Config->FailureLineTemplate);
		}

		Widget->ShowResult(Line, bSuccess || Outcome == EClcHaggleOutcome::Accepted);
	}
}

void UClcHaggleComponent::FinishResolve()
{
	const EClcHaggleOutcome Out = ResolvedOutcome;
	const int32 Price = ResolvedFinalPrice;
	const float Ratio = AppliedRatio;

	Phase = EClcHagglePhase::Idle;
	CloseWidget();

	OnHaggleResolved.Broadcast(Out, Price, Ratio);
}

// ============================================================
// Tick
// ============================================================

void UClcHaggleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (Phase)
	{
	case EClcHagglePhase::Selection: TickSelection(); break;
	case EClcHagglePhase::Playing:   TickPlaying(DeltaTime); break;
	case EClcHagglePhase::Resolved:  TickResolved(DeltaTime); break;
	default: break; // Idle
	}
}

void UClcHaggleComponent::TickSelection()
{
	APlayerController* PC = CachedPC.Get();
	if (!PC) return;

	auto Edge = [&](FKey Key, bool& bPrev) -> bool
	{
		const bool bDown = PC->IsInputKeyDown(Key);
		const bool bJust = bDown && !bPrev;
		bPrev = bDown;
		return bJust;
	};

	// 数字键 1~6 选档
	static const FKey NumKeys[6] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
	const TArray<FClcHaggleTier>& Tiers = ResolveTiers();
	const int32 MaxScan = FMath::Min<int32>(6, Tiers.Num());
	for (int32 i = 0; i < MaxScan; ++i)
	{
		if (Edge(NumKeys[i], bNumPrev[i]))
		{
			ChooseTier(i);
			return;
		}
	}

	// 空格 → 直接出手
	if (Edge(EKeys::SpaceBar, bSpacePrev))
	{
		ChooseAccept();
		return;
	}
}

void UClcHaggleComponent::TickPlaying(float DeltaTime)
{
	APlayerController* PC = CachedPC.Get();
	if (!PC || Sequence.Num() == 0) return;

	// 计时仅在玩家按下第一个正确键后启动：先进「展示序列」预备态，不计超时。
	if (bTimerStarted)
	{
		PerKeyTimer -= DeltaTime;
		if (PerKeyTimer <= 0.0f)
		{
			Resolve(EClcHaggleOutcome::Failure); // 超时
			return;
		}
	}

	auto Edge = [&](FKey Key, bool& bPrev) -> bool
	{
		const bool bDown = PC->IsInputKeyDown(Key);
		const bool bJust = bDown && !bPrev;
		bPrev = bDown;
		return bJust;
	};

	const bool kW = Edge(EKeys::W, bWPrev);
	const bool kA = Edge(EKeys::A, bAPrev);
	const bool kS = Edge(EKeys::S, bSPrev);
	const bool kD = Edge(EKeys::D, bDPrev);
	// 同时兼容方向键
	const bool kUp    = Edge(EKeys::Up,    bUpPrev);
	const bool kLeft  = Edge(EKeys::Left,  bLeftPrev);
	const bool kDown  = Edge(EKeys::Down,  bDownPrev);
	const bool kRight = Edge(EKeys::Right, bRightPrev);

	int32 PressedCode = -1;
	if (kW || kUp)         PressedCode = 0; // ↑
	else if (kA || kLeft)  PressedCode = 1; // ←
	else if (kS || kDown)  PressedCode = 2; // ↓
	else if (kD || kRight) PressedCode = 3; // →

	if (PressedCode >= 0)
	{
		const uint8 Expected = Sequence[CurrentKeyIndex];
		if (PressedCode == static_cast<int32>(Expected))
		{
			CurrentKeyIndex++;
			if (CurrentKeyIndex >= Sequence.Num())
			{
				Resolve(EClcHaggleOutcome::Success);
				return;
			}
			PerKeyTimer = Config->TimePerKey; // 下一键窗口
			bTimerStarted = true;             // 首个正确键启动倒计时
		}
		else
		{
			Resolve(EClcHaggleOutcome::Failure); // 按错
			return;
		}
	}

	if (Widget)
	{
		// 预备态（未启动）显示满条；启动后按剩余比例
		const float Frac = (!bTimerStarted || Config->TimePerKey <= 0.0f)
			? 1.0f
			: PerKeyTimer / Config->TimePerKey;
		Widget->UpdatePlaying(CurrentKeyIndex, Frac);
	}
}

void UClcHaggleComponent::TickResolved(float DeltaTime)
{
	ResolveTimer -= DeltaTime;
	if (ResolveTimer <= 0.0f)
	{
		FinishResolve();
	}
}
