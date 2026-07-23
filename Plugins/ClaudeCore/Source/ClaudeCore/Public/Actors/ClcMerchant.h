// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcMerchantTypes.h"
#include "ClcMerchant.generated.h"

class AClcStoneStall;
class AClcStone;
class UClcMerchantConfig;
class UClcMerchantAnimConfig;
class UClcMerchantBubbleConfig;
class UClcMerchantTalkConfig;
class UClcMerchantPersonality;
class UClcMerchantBubbleWidget;
class UClcMerchantEagleEyeWidget;
class USkeletalMeshComponent;
class USphereComponent;
class UAnimSequence;
class APawn;

/**
 * 商人 NPC——绑定摊位，四通道反馈供玩家交叉博弈：
 *   1. 身体动作：整摊 mood（可演）+ 单块微反应（诚实泄漏，受演技 gate 调节泄漏强度）
 *   2. 嘴上话术：走近显示，随瞄准/购入/离开更新，可骗（按撒谎倾向决定声称档位）
 *   3. 性格 tag：鹰眼可见固有标签，驱动该商人的撒谎倾向 + 演技
 *   4. 心理话：鹰眼限时，诚实（原气泡重新定位）
 *
 * 口头气泡与鹰眼洞察使用独立 Widget，可在玩家处于话术范围内时同屏显示。
 * 鹰眼开启/结束只影响洞察 UI，不覆盖或销毁口头气泡。
 *
 * 生命周期：由 AClcStoneStall spawn + Initialize。鹰眼通过 ShowBubble/HideBubble 切鹰眼模式。
 */
UCLASS()
class CLAUDECORE_API AClcMerchant : public AActor
{
	GENERATED_BODY()

public:
	AClcMerchant();

	// ---- 外部接口 ----

	/** 由摊位调用：绑定摊位 + 加载配置 + roll 性格 + 初始动画 */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	void Initialize(AClcStoneStall* Stall);

	/** 鹰眼激活时调——显示独立心理话与性格洞察 UI */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	void ShowBubble();

	/** 鹰眼结束时调——仅隐藏洞察 UI，不影响口头气泡 */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	void HideBubble();

	/** 当前是否显示任一商人 UI */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	bool IsBubbleVisible() const { return TalkBubbleWidget != nullptr || EagleEyeWidget != nullptr; }

	/** 获取当前性格（可能为空） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	UClcMerchantPersonality* GetPersonality() const { return Personality; }

	/** 摊位石头铺好后由摊位调用——重算档位（SpawnMerchant 时空摊会被误判成 Bad） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	void RecomputeTier();

	/** 商人欺骗倾向 [0,1]（无性格兜底 0.5） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchant")
	float GetDeceptionLevel() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcMerchant")
	USkeletalMeshComponent* Mesh;

	/** 嘴上话术范围触发器——玩家进入显示嘴上气泡，只响应本地 Pawn */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcMerchant")
	USphereComponent* TalkTrigger;

private:
	// ---- 配置 ----
	UPROPERTY()
	UClcMerchantConfig* Config = nullptr;

	UPROPERTY()
	UClcMerchantAnimConfig* AnimConfig = nullptr;

	UPROPERTY()
	UClcMerchantBubbleConfig* BubbleConfig = nullptr;

	UPROPERTY()
	UClcMerchantTalkConfig* TalkConfig = nullptr;

	UPROPERTY()
	UClcMerchantPersonality* Personality = nullptr;

	// ---- 关联 ----
	UPROPERTY()
	TWeakObjectPtr<AClcStoneStall> BoundStall;

	// ---- 状态 ----
	EClcStallTier CurrentTier = EClcStallTier::Mid;
	EClcPurchaseOutcome LastOutcome = EClcPurchaseOutcome::None;

	/** 玩家当前瞄准的石头（nullptr=没瞄准） */
	UPROPERTY()
	TWeakObjectPtr<AClcStone> CurrentAimedStone;

	bool bInMicroReaction = false;
	float ReactionTimer = 0.0f;

	/** Aim 态持续时节律重播微反应的倒计时——球扫后瞄准稳定，不再靠抖动触发，需主动节律 */
	float MicroReactionRetriggerTimer = 0.0f;
	float MoodReshuffleTimer = 0.0f;

	/** 商人高频 Tick 时将瞄准 trace 限制为每 0.1 秒一次 */
	float AimTraceTimer = 0.0f;

	/** ClcMerchant.DebugBubble 汇总日志的实例级限频计时器 */
	float DebugBubbleLogTimer = 0.0f;

	/** 购买反馈保留期间继续跟踪瞄准目标，但不允许 Aim/Enter 覆盖 Purchase 文本 */
	float PurchaseFeedbackTimer = 0.0f;

	// ---- 嘴上话术 / 气泡状态 ----
	bool bEagleEyeActive = false;
	ETalkState CurrentTalkState = ETalkState::Enter;
	UPROPERTY()
	TWeakObjectPtr<APawn> PlayerInRange;

	/** 缓存的整摊声称档位——商人一旦决定演某档就稳定，避免每帧重 roll 跳变；档位变化时失效 */
	EClcStallTier CachedClaimedTier = EClcStallTier::Mid;
	bool bClaimedTierValid = false;

	// ---- UI ----
	UPROPERTY()
	UClcMerchantBubbleWidget* TalkBubbleWidget = nullptr;

	UPROPERTY()
	UClcMerchantEagleEyeWidget* EagleEyeWidget = nullptr;

	// ---- 内部方法 ----
	void LoadConfigs();
	/** 贴地——从当前位置向下 trace 找地面，落 Z */
	void SnapToGround();
	void PlayMoodAnim();
	void PlayMicroReactionForStone(AClcStone* Stone);
	void TickAimedStone();
	void OnAimedStoneChanged(AClcStone* NewStone);

	/** 带 blend 的动画播放——用 dynamic montage 代替 PlayAnimation 的瞬切 */
	void PlayAnimWithBlend(UAnimSequence* Anim, bool bLoop);

	/** 计算嘴上「声称档位」——撒谎倾向决定非好摊被说成好摊的概率；结果缓存至档位变化 */
	EClcStallTier ComputeClaimedTier();

	/** 分别刷新口头气泡与鹰眼洞察，二者生命周期互不影响 */
	void RefreshTalkBubble();
	void RefreshEagleEyeWidget();
	void EnsureTalkBubbleWidget();
	void EnsureEagleEyeWidget();
	void DestroyTalkBubbleWidget();
	void DestroyEagleEyeWidget();
	void UpdateWidgetTickInterval();

	/** TriggerSphere overlap 回调——走近/离开触发嘴上气泡 */
	UFUNCTION()
	void OnTalkTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnTalkTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 摊位石头移除回调——摊位算好购买结果传过来 */
	UFUNCTION()
	void OnStoneRemoved(EClcPurchaseOutcome Outcome);
};
