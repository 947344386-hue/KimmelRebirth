// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ClcHaggleConfig.h"
#include "ClcHaggleComponent.generated.h"

class UClcHaggleWidget;
class UClcHaggleConfig;
class APlayerController;
class UClcStoneMarketSubsystem;

/** 讨价还价开始（进入选择阶段）——vendor 绑此播 NPC 报价演绎 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FClcOnHaggleOpened);

/** 讨价还价结算——vendor 绑此完成售出/回退 + 演绎 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FClcOnHaggleResolved,
	EClcHaggleOutcome, Outcome, int32, FinalPrice, float, AppliedRatio);

/**
 * 讨价还价 QTE 组件——挂在 AClcStoneVendor 上，独占 QTE 状态机/输入/Widget。
 *
 * 相位：Idle → Selection（选档：数字键 1~N 加价 / 空格直接出手 / Esc 取消）
 *           → Playing（WASD 序列边沿检测，对则前进，错/超时则失败）
 *           → Resolved（展示结果 ResolveDelay 秒）→ Idle（广播 OnHaggleResolved）
 *
 * QTE 键编码：0=W(↑) 1=A(←) 2=S(↓) 3=D(→)
 */
UCLASS(ClassGroup = (Clc), meta = (BlueprintSpawnableComponent))
class CLAUDECORE_API UClcHaggleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcHaggleComponent();

	UPROPERTY(BlueprintAssignable, Category = "ClcHaggle")
	FClcOnHaggleOpened OnHaggleOpened;

	UPROPERTY(BlueprintAssignable, Category = "ClcHaggle")
	FClcOnHaggleResolved OnHaggleResolved;

	/** 开始讨价还价（vendor 在 RequestSell 调）：加载配置/开 Widget/进选择阶段 */
	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	void StartHaggle(int32 InReferencePrice, APlayerController* PC);

	/** 选择阶段取消（vendor Esc 轮询调；非选择阶段忽略） */
	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	void RequestCancel();

	/** Esc 统一入口：选择阶段→取消回查看；QTE 阶段（未失败）→回选择重选/直接售出；结算中→忽略 */
	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	void RequestEsc();

	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	bool IsInSelectionPhase() const { return Phase == EClcHagglePhase::Selection; }

	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	bool IsHaggleActive() const { return Phase != EClcHagglePhase::Idle; }

	/** 取已加载的配置（首次调用触发懒加载；vendor 用来取 NPC 资产） */
	UFUNCTION(BlueprintCallable, Category = "ClcHaggle")
	UClcHaggleConfig* GetHaggleConfig();

	// ---- Widget 回调（C++ 直接调用，无需反射） ----
	void ChooseAccept();
	void ChooseTier(int32 TierIndex);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	enum class EClcHagglePhase : uint8 { Idle, Selection, Playing, Resolved } Phase = EClcHagglePhase::Idle;

	UClcHaggleConfig* LoadConfig();
	const TArray<FClcHaggleTier>& ResolveTiers() const;
	UClcStoneMarketSubsystem* GetMarket() const;

	void OpenWidget(APlayerController* PC);
	void CloseWidget();
	void BeginSelection();
	void BeginTierSequence(int32 TierIndex);
	void RetreatToSelection();
	void Resolve(EClcHaggleOutcome Outcome);
	void FinishResolve();

	void TickSelection();
	void TickPlaying(float DeltaTime);
	void TickResolved(float DeltaTime);

	uint8 RandomKeyCode() const;

	UPROPERTY(Transient)
	TObjectPtr<UClcHaggleWidget> Widget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UClcHaggleConfig> Config = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> CachedPC;

	int32 ReferencePrice = 0;
	float AppliedRatio = 0.0f;

	// ---- Playing 状态 ----
	TArray<uint8> Sequence;
	int32 CurrentKeyIndex = 0;
	float PerKeyTimer = 0.0f;

	/** 第一个正确键按下前不计时（先展示序列，给玩家反应时间） */
	bool bTimerStarted = false;

	// ---- Resolved 状态 ----
	float ResolveTimer = 0.0f;
	int32 ResolvedFinalPrice = 0;
	EClcHaggleOutcome ResolvedOutcome = EClcHaggleOutcome::Cancelled;

	// ---- 边沿检测（自维护，避免输入模式切换重置 WasInputKeyJustPressed） ----
	bool bWPrev = false, bAPrev = false, bSPrev = false, bDPrev = false;
	bool bUpPrev = false, bDownPrev = false, bLeftPrev = false, bRightPrev = false;
	bool bSpacePrev = false;
	bool bNumPrev[6] = { false, false, false, false, false, false };
};
