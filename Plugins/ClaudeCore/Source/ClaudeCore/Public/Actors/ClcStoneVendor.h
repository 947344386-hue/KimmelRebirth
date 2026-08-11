// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Actors/ClcInteractableStation.h"
#include "Data/ClcJadeTypes.h"
#include "Data/ClcHaggleConfig.h"
#include "UI/ClcVendorHUD.h"
#include "ClcStoneVendor.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UArrowComponent;
class USphereComponent;
class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UClcInteractionIndicator;
class UClcBackpackSubsystem;
class UClcHaggleComponent;
class AClcOpeningStone;
class AClcCuttingStone;

/**
 * 回收台 Actor——出售入口。
 * 玩家进入范围按 F → 背包选石 → 按 Phase 分支 Spawn 上台展示（OpeningStone/CuttingStone）
 * → WASD 旋转查看 + HUD 显示擦石/解石情况/回收价 → 按 Enter 或 UI 售出按钮才真正售出。
 *
 * 架构 B：旋转展示与讨价还价由出售台自管，石头 Actor 只管 Initialize + GetStoneData。
 *
 * 蓝图定制：VendorMesh、PromptText、InteractionRadius、EnterKey、相机组件定位、
 *           HUDWidgetClass、FillLight 档位、OnStoneSold/OnEnter/OnExit（音效特效）
 */
UCLASS(Blueprintable, ClassGroup = (Clc))
class CLAUDECORE_API AClcStoneVendor : public AClcInteractableStation, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcStoneVendor();

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

	/** 查询当前是否处于出售模式（活跃 = 非 Inactive） */
	UFUNCTION(BlueprintCallable, Category = "ClcVendor")
	bool IsInSellMode() const { return CurrentState != EClcVendorState::Inactive; }

	/** 是否处于等待选石状态 */
	UFUNCTION(BlueprintCallable, Category = "ClcVendor")
	bool IsAwaitingStone() const { return CurrentState == EClcVendorState::AwaitingStone; }

	/** 台上是否有石 */
	UFUNCTION(BlueprintCallable, Category = "ClcVendor")
	bool IsStoneOnBench() const { return CurrentState == EClcVendorState::StoneOnBench; }

	/**
	 * 售出请求——键盘 Enter 与 HUD 售出按钮共用入口。仅 StoneOnBench 响应。
	 * 预留小游戏 hook：将来在此启动小游戏，完成后回调 CompleteSell。
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcVendor")
	void RequestSell();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	/** 无缩放根——VendorMesh/TriggerSphere/Indicator/相机都挂这下面，避免 BP 缩放污染 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* VendorRoot;

	/** 出售点外观 Mesh——蓝图继承后定制（柜台、NPC、招牌等） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VendorMesh;

	/** 范围触发器——检测玩家进出范围（只响应 Pawn Overlap） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	/** 交互指示器——范围内+背包有石头→选中状态（bSelectByProximity 模式） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	/** 石头生成定位点——上台的石头挂这下面 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* StoneSpawnPoint;

	/** 观察相机摇臂——挂 StoneSpawnPoint，bDoCollisionTest=false */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraArm;

	/** 观察相机——展示模式切到此相机看石头 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* VendorCamera;

	/** NPC 站位锚点（蓝图可拖动）——箭头位置=NPC 站位，箭头朝向=NPC 面朝方向；NpcMesh 挂其下并在运行时贴地 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UArrowComponent* NpcSpawnPoint;

	/** NPC 网格体——挂 NpcSpawnPoint；资产在 DA_HaggleConfig 配，C++ 自动 SetSkeletalMesh+PlayAnimation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* NpcMesh;

	/** 讨价还价 QTE 组件——独占 QTE 状态机/输入/Widget */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcHaggleComponent* HaggleComponent;

	// ---- 配置 ----

	/** 交互提示文字 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FText PromptText = FText::FromString(TEXT("按 F 出售石头"));

	/** 交互半径（运行时同步到 TriggerSphere 和 InteractionIndicator） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "100.0"))
	float InteractionRadius = 300.0f;

	/** 进入出售模式的按键（默认 F） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey EnterKey = FKey("F");

	/** 退出出售模式的按键（默认 Escape） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey ExitKey = FKey("Escape");

	/** 售出按键（默认 Enter）——HUD 售出按钮与其共用 RequestSell */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey SellKey = FKey("Enter");

	/** 旋转复位键（平滑回到石头初始朝向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FKey ResetRotationKey = FKey("R");

	/** 旋转复位平滑速度（越大越快，8≈0.15秒到位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float ResetRotationSpeed = 8.0f;

	/** 相机距石头距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	float CameraDistance = 200.0f;

	/** WASD 旋转速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	float RotationInputScale = 1.0f;

	/** 解石台外壳材质路径（AClcCuttingStone 初始化时加载） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FString ShellMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneShell.M_StoneShell");

	/** 解石台体素分辨率（AClcCuttingStone 初始化时传入） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "16", ClampMax = "96"))
	int32 VoxelResolution = 48;

	/** 解石台缺陷覆盖率回落值（AClcCuttingStone 初始化时传入） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "0.01", ClampMax = "0.95"))
	float FallbackDefectCoverage = 0.18f;

	/** 台上石头旋转速度（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	float DisplayRotationSpeed = 90.0f;

	/** 擦石材质路径（AClcOpeningStone 初始化时加载） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	FString OpeningMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneOpening.M_StoneOpening");

	/** 启用讨价还价 QTE（false=Enter 直接按参考价售出，旧行为） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config")
	bool bEnableHaggle = true;

	/** NPC 自动贴地的向下探测距离（从 NpcSpawnPoint 往下找地面） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Config", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
	float GroundSnapTraceDistance = 300.0f;

	// ---- HUD ----

	/** Vendor HUD Widget 类（BP 端创建排版，C++ 推数据） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|HUD")
	TSubclassOf<UClcVendorHUD> HUDWidgetClass;

	/** HUD 数据推送间隔（秒，默认 0.3） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|HUD")
	float HUDPushInterval = 0.3f;

	// ---- 自适应补光强度档位（位置/锥角/颜色在 BP 的 FillLight 组件上调） ----

	/** 未进入展示模式时的补光强度（0=灭） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|FillLight", meta = (ClampMin = "0.0"))
	float FillLightInactiveIntensity = 0.0f;

	/** 展示/选石时的补光强度（看清石头） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|FillLight", meta = (ClampMin = "0.0"))
	float FillLightDisplayIntensity = 5000.0f;

	// ---- tips 文案（EditAnywhere 便于策划改，运行时格式化） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText EnterTip = FText::FromString(TEXT("进入回收台——选择一块石头上台"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText EmptyTip = FText::FromString(TEXT("背包空，无可回收的石头"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText PlacedTip = FText::FromString(TEXT("上台：{0}"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText SwapTip = FText::FromString(TEXT("换石：已放回 {0}"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText SoldTip = FText::FromString(TEXT("售出 {0}，获得 {1} 金币"));

	/** 讨价还价结束后锁定售价的提示（{0}=锁定价） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText HaggleLockedTip = FText::FromString(TEXT("讨价结束：最终售价 {0} 金（已锁定，Enter 出手）"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText SoldEmptyExitTip = FText::FromString(TEXT("背包已空，已退出回收台"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText SoldHasMoreTip = FText::FromString(TEXT("选择下一块石头"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcVendor|Tips")
	FText ExitTip = FText::FromString(TEXT("已退出回收台"));

	// ---- 蓝图事件（蓝图覆写以播放音效/特效/动画） ----

	/** 石头出售成功——蓝图可播音效/特效/粒子 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnStoneSold(const FClcStoneRuntimeData& StoneData, int32 SalePrice);

	/** 进入出售模式——蓝图可播进入动画/提示 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnEnterSellMode();

	/** 退出出售模式——蓝图可播退出动画 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events")
	void OnExitSellMode();

	// ---- 讨价还价 NPC 演绎钩子（BP 覆写，在 NpcMesh 上播 Montage/音效） ----

	/** NPC 报价演绎——讨价还价 UI 开启时 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|Haggle")
	void OnNpcMakeOffer();

	/** 加价成功演绎（FinalPrice=最终成交价） */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|Haggle")
	void OnNpcHaggleSuccess(int32 FinalPrice);

	/** 直接出手演绎（FinalPrice=成交价） */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|Haggle")
	void OnNpcHaggleAccept(int32 FinalPrice);

	/** 加价失败演绎 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|Haggle")
	void OnNpcHaggleFail(int32 FinalPrice);

	/** 玩家取消演绎 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|Haggle")
	void OnNpcHaggleCancel();

	// ---- 高价值 NPC 反馈事件（C++ 自动调用，空实现=无动作；BP 可覆写加音效/特效） ----

	/** 石头上台——priceTrend: 1=涨 0=持平 -1=跌；openStatus: 0=未开 1=纯杂 2=见玉；isLocked: 是否锁价石 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|NPC")
	void OnNpcStonePlaced(int32 PriceTrend, int32 OpenStatus, bool bIsLocked);

	/** 石头上台失败（Spawn/Init 失败） */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|NPC")
	void OnNpcStonePlaceFailed();

	/** 售出完成——priceTrend 同上台语义，按最终成交价判 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|NPC")
	void OnNpcSold(int32 PriceTrend, bool bSoldAll);

	/** 进入出售模式 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|NPC")
	void OnNpcEnterSellMode();

	/** 退出出售模式——bSoldAll: 是否因售空退出 */
	UFUNCTION(BlueprintNativeEvent, Category = "ClcVendor|Events|NPC")
	void OnNpcExitSellMode(bool bSoldAll);

private:
	// ---- 状态机 ----

	enum class EClcVendorState : uint8
	{
		Inactive,
		AwaitingStone,
		StoneOnBench,
		Haggling
	};

	EClcVendorState CurrentState = EClcVendorState::Inactive;

	/** 台上石头——可能是 AClcOpeningStone（擦石/未加工）或 AClcCuttingStone（已解石） */
	UPROPERTY()
	TObjectPtr<AActor> BenchStone = nullptr;

	/** 台上石头的旋转 Mesh（出售台自己管旋转，不通过石头 Actor 方法） */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> BenchDisplayMesh;

	/** 台上石头初始旋转（用于 R 键复位） */
	FQuat BenchInitialRotation = FQuat::Identity;

	/** 台上石头 Phase 缓存（GetStoneData 分发用） */
	EClcStonePhase BenchStonePhase = EClcStonePhase::Unworked;

	FClcStoneRuntimeData ActiveStoneData;

	// ---- HUD ----

	UPROPERTY()
	UClcVendorHUD* HUDWidget = nullptr;

	/** NPC 当前台词——高价值事件点填入，PushVendorHUDData 写入 Data.NpcLine 并累积停留计时 */
	FString PendingNpcLine;

	/** 上一次推到 HUD 的台词文本——用于在 PushVendorHUDData 中检测新台词出现并重置停留计时 */
	FString LastPushedNpcLine;

	/** 当前台词已停留时间（秒），>= NpcLineMinDuration 后 NpcLine 置空隐藏对话框 */
	float NpcLineElapsed = 0.0f;

	/** 填入新台词并重置停留计时（有词立即刷掉旧词） */
	void SetNpcLine(const FString& Line);

	/** 每帧 Tick 台词停留计时，到期自动隐藏对话框 */
	void TickNpcLine(float DeltaTime);

	float HUDPushTimer = 0.0f;

	// ---- 输入边沿检测（自维护，避免输入模式切换重置 WasInputKeyJustPressed） ----

	bool bExitKeyPrev = false;
	bool bSellKeyPrev = false;
	bool bRKeyPrev = false;
	bool bResetRotationPending = false;
	bool bBackpackWasOpen = false;

	/** 按键提示句柄：进入范围注册 F（出售），离开/EndPlay 注销 */
	int32 VendorPromptHandle = 0;

	/** 进入范围飘字冷却（per-instance，防 overlap 抖动；多台回收台各自计时） */
	double LastEnterToastTime = 0.0;

	// ---- 内部流程 ----

	void EnterSellMode();
	void ExitSellMode();
	void PlaceStoneOnVendor(int32 StoneIndex);
	void RemoveStoneFromVendor();
	void DestroyBenchStone();

	/** 从台上石头（任意类型）获取运行时数据 */
	bool GetBenchStoneData(FClcStoneRuntimeData& OutData) const;

	/** 讨价还价：锁售价到 ActiveStoneData + 更新 DisplayName */
	void MarkHaggleResolvedOnActiveStone(int32 LockedPrice);

	void CompleteSell();
	/** 用指定价格完成售出（讨价还价结算价走这里；CompleteSell 重算参考价后调本函数） */
	void CompleteSellWithPrice(int32 Price);

	// ---- 讨价还价回调（HaggleComponent 多播驱动） ----

	UFUNCTION()
	void HandleHaggleOpened();

	UFUNCTION()
	void HandleHaggleResolved(EClcHaggleOutcome Outcome, int32 FinalPrice, float AppliedRatio);

	/** 讨价成功/失败后：锁定售价到石头、回查看态、刷新 HUD 显示锁价（不自动售出） */
	void LockHagglePriceAndReturn(int32 LockedPrice);

	// ---- NPC 演绎（C++ 自动播放，资产来自 HaggleConfig） ----

	/** BeginPlay 用配置给 NpcMesh 赋网格 + 循环待机动画 */
	void SetupNpcFromConfig();

	/** 从 NpcSpawnPoint 向下 trace 找地面，把 NpcMesh 脚贴地 */
	void SnapNpcToGround();

	/** 在 NpcMesh 上播一段动画；非循环时播完自动回 Idle */
	void PlayNpcAnim(class UAnimSequence* Anim, bool bLoop);

	UFUNCTION()
	void ReturnToNpcIdle();

	/** 状态动画播完后回 Idle 的定时器 */
	FTimerHandle NpcReturnIdleTimer;
	void ProcessStoneOnBenchInput(float DeltaTime);

	// ---- HUD ----
	void CreateVendorHUD();
	void DestroyVendorHUD();
	void PushVendorHUDData();

	// ---- 背包 ----
	virtual void OnBackpackStoneSelected(int32 StoneIndex) override;

	// ---- 光标 ----
	void SetVendorCursor(bool bVisible);

	// ---- 补光 ----
	virtual void UpdateFillLightTarget() override;
	virtual UCameraComponent* GetAimZoomCamera() const override;

	// ---- 委托 / 触发器 ----

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
