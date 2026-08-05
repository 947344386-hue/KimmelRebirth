// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "Interfaces/ClcInteractable.h"
#include "ClcJadeWorkbench.generated.h"

/** 工具模式（BP 可用 Switch on EClcToolMode） */
UENUM(BlueprintType)
enum class EClcToolMode : uint8
{
	Opener     UMETA(DisplayName = "开窗器"),
	Flashlight UMETA(DisplayName = "手电筒"),
	Combined   UMETA(DisplayName = "手电开窗器")
};

class USphereComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UClcInteractionIndicator;
class AClcOpeningStone;
class AClcStoneTool;
class UClcWorkbenchHUD;
struct FClcWorkbenchHUDData;

/**
 * 工作台 Actor——原石开窗的入口。
 * 玩家进入范围按键 → 背包选石 → Spawn AClcOpeningStone → WASD 旋转 + 工具操作 → 按键退出回背包。
 *
 * 工具系统：T 键循环切换工具模式（开窗器 ⇄ 手电筒），互斥。
 * 每帧做鼠标 LineTrace，命中石头时把结果传给当前工具的 OnUpdate。
 */
UCLASS()
class CLAUDECORE_API AClcJadeWorkbench : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcJadeWorkbench();

	/** 当前是否处于开窗模式 */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	bool IsInOpeningMode() const { return CurrentState != EClcWorkbenchState::Inactive; }

	/** 获取当前在工作台上的石头数据（可能为空，返回 bool 表示是否有效） */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	bool GetActiveStone(FClcStoneRuntimeData& OutData) const;

	/** 查询当前石头是否有效 */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	bool HasActiveStone() const { return CurrentState == EClcWorkbenchState::StoneOnBench; }

	/** 开窗模式是否处于等待选石状态 */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	bool IsAwaitingStoneSelection() const { return CurrentState == EClcWorkbenchState::AwaitingStone; }

	/** 石头是否已放置在工作台上 */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	bool IsStoneOnBench() const { return CurrentState == EClcWorkbenchState::StoneOnBench; }

	/** 获取当前台上的 AClcOpeningStone（蓝图可用于 UI 查询等） */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	AClcOpeningStone* GetOpeningStone() const { return OpeningStone; }

	/** 获取当前激活的工具 */
	UFUNCTION(BlueprintCallable, Category = "ClcWorkbench")
	AClcStoneTool* GetCurrentTool() const { return CurrentTool; }

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* BenchRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* BenchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* StoneSpawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* WorkCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* CameraArm;

	/** 交互指示器——范围内+背包有石头→选中态（与出售台一致） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	/**
	 * 自适应补光——按当前状态/工具调节强度，避免太亮或太暗。
	 * 位置/锥角/颜色等直接在 BP 的 FillLight 组件上调；这里只暴露强度档位。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpotLightComponent* FillLight;

	// ---- 配置 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	float TriggerRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FKey EnterKey = FKey("F");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FKey ExitKey = FKey("Escape");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FKey BackpackKey = FKey("B");

	/** 工具切换键（循环：开窗器 ⇄ 手电筒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FKey ToolSwitchKey = FKey("T");

	/** 旋转复位键（平滑回到石头初始朝向） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FKey ResetRotationKey = FKey("R");

	/** 旋转复位平滑速度（越大越快，8≈0.15秒到位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float ResetRotationSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FText InteractionPrompt = FText::FromString(TEXT("Press [F] to use Workbench"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	float CameraDistance = 200.0f;

	/** 长按右键放大倍率（2.0=放大2倍，只改 FOV，不碰开窗/工具/手电筒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float AimZoomFactor = 2.0f;

	/** 放大过渡速度（越大越快，10≈0.1秒到位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float AimZoomSpeed = 10.0f;

	/** 开窗材质路径（AClcOpeningStone 初始化时加载） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FString OpeningMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneOpening.M_StoneOpening");

	/** WASD 旋转速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	float RotationInputScale = 1.0f;

	// ---- 自适应补光强度档位（位置/锥角/颜色在 BP 的 FillLight 组件上调） ----

	/** 未进入工作台时的补光强度（0=灭，不照亮环境） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float FillLightInactiveIntensity = 0.0f;

	/** 进入但还没放石头时的补光强度（看清台面） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float FillLightIdleIntensity = 2500.0f;

	/** 开窗器模式补光强度（亮，看清表面精磨） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float OpenerFillLightIntensity = 7000.0f;

	/** 手电筒选中但未开灯时的补光强度（中等，避免画面太暗） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float FlashlightIdleFillLightIntensity = 3000.0f;

	/** 手电筒开灯透视时的补光强度（压暗，让手电光锥和 X-ray 突出） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float FlashlightActiveFillLightIntensity = 800.0f;

	/** 补光强度过渡速度（越大越快，0=瞬切，建议 8~12） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|FillLight", meta = (ClampMin = "0.0"))
	float FillLightTransitionSpeed = 10.0f;

	// ---- 工具蓝图槽位（指定 BP 子类来覆写参数 / Mesh / 表现） ----

	/** 开窗器工具类——默认用 C++ 类，可改为 BP 子类覆写 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Tools")
	TSubclassOf<AClcStoneTool> OpeningToolClass;

	/** 手电筒工具类——默认用 C++ 类，可改为 BP 子类覆写 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Tools")
	TSubclassOf<AClcStoneTool> FlashlightToolClass;

	/** 组合工具（手电开窗器）类——拥有升级后工作台改用此类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Tools")
	TSubclassOf<AClcStoneTool> CombinedToolClass;

	// ---- HUD 蓝图槽位 ----

	/** Workbench HUD Widget 类（BP 端创建排版，C++ 推数据） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|HUD")
	TSubclassOf<UClcWorkbenchHUD> HUDWidgetClass;

	/** HUD 数据推送间隔（秒，默认 0.3） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|HUD")
	float HUDPushInterval = 0.3f;

	// ---- 蓝图可覆写 ----

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void OnEnterOpeningMode();

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void OnExitOpeningMode();

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void OnStonePlaced(const FClcStoneInternalData& StoneData);

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void OnStoneRemoved();

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void OnToolModeChanged(EClcToolMode NewMode);

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void ShowPrompt(const FText& PromptText);

	UFUNCTION(BlueprintNativeEvent, Category = "Workbench|Events")
	void HidePrompt();

private:
	// ---- HUD ----

	/** 组装完整数据包并推给 BP Widget */
	void PushHUDData();

	/** 创建 HUD Widget 实例 */
	void CreateHUD();
	void DestroyHUD();

	UPROPERTY()
	UClcWorkbenchHUD* HUDWidget = nullptr;

	float HUDPushTimer = 0.0f;

	/** 种水是否已暴露（首次开到绿时设为 true） */
	bool bGradeRevealed = false;

	/** 补光当前强度（每帧平滑追向 Target） */
	float CurrentFillLightIntensity = 0.0f;
	/** 补光目标强度（由 UpdateFillLightTarget 按状态算出） */
	float TargetFillLightIntensity = 0.0f;

private:
	enum class EClcWorkbenchState : uint8
	{
		Inactive,
		AwaitingStone,
		StoneOnBench
	};

	EClcToolMode CurrentToolMode = EClcToolMode::Opener;

	UPROPERTY()
	AClcStoneTool* CurrentTool = nullptr;

	bool bLeftMousePrev = false;

	/** 进入开窗时缓存的基础 FOV（右键放大基于此值缩放，退出时恢复） */
	float BaseFOV = 90.0f;

	// ---- 按键边沿检测（自维护，避免输入模式切换重置 WasInputKeyJustPressed） ----
	bool bExitKeyPrev = false;
	bool bTKeyPrev = false;
	bool bRKeyPrev = false;

	/** R 键旋转复位激活中（每帧 Tick 平滑追初始朝向） */
	bool bResetRotationPending = false;

	/** 笔刷边界 Toast 冷却（防止滚轮到底时刷屏） */
	double LastBrushBoundaryToastTime = 0.0;
	static constexpr double BrushBoundaryToastCD = 1.5;

	/** 背包开闭状态（轮询用，检测全局 IA_Backpack 触发的开关） */
	bool bBackpackWasOpen = false;

	/** 按键提示句柄：进入范围注册 F（使用工作台），离开/EndPlay 注销 */
	int32 WorkbenchPromptHandle = 0;

	/** 进入范围飘字冷却（per-instance，防 overlap 抖动；多台工作台各自计时） */
	double LastEnterToastTime = 0.0;

	// ---- 工作台状态 ----

	EClcWorkbenchState CurrentState = EClcWorkbenchState::Inactive;

	UPROPERTY()
	AClcOpeningStone* OpeningStone = nullptr;

	FClcStoneRuntimeData ActiveStoneData;

	UPROPERTY()
	TWeakObjectPtr<APawn> PlayerInRange;

	UPROPERTY()
	TWeakObjectPtr<APlayerController> CachedPC;

	TWeakObjectPtr<UObject> CachedCarrierObj;
	class IClcStoneCarrier* CachedCarrier = nullptr;

	// ---- 内部流程 ----

	void CachePlayerRefs();
	void EnterOpeningMode();
	void ExitOpeningMode();
	void PlaceStoneOnBench(int32 StoneIndex);
	void RemoveStoneFromBench();
	void DestroyOpeningStone();
	void BindToBackpackWidget();
	void ProcessStoneOnBenchInput(float DeltaTime);
	/** 长按右键 FOV 放大（独立于工具，纯视觉拉近） */
	void UpdateAimZoom(float DeltaTime);
	void SetWorkbenchCursor(bool bVisible);

	// ---- 工具管理 ----

	void CycleToolMode();
	void SwitchToolMode(EClcToolMode NewMode);
	void SpawnCurrentTool();
	void DestroyCurrentTool();

	// ---- 自适应补光 ----

	/** 按当前状态/工具/手电开关重算目标强度 */
	void UpdateFillLightTarget();
	/** 每帧把当前强度平滑追向目标并应用到 FillLight */
	void TickFillLight(float DeltaTime);

	UFUNCTION()
	void OnBackpackStoneSelected(int32 StoneIndex);

	/** InteractionIndicator 委托——背包有石头返回 true（选中态），否则 false（范围内态） */
	UFUNCTION()
	bool QueryCanSelect();

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
