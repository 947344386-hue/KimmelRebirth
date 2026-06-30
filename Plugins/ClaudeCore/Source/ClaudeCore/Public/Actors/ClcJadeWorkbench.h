// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "ClcJadeWorkbench.generated.h"

/** 工具模式（BP 可用 Switch on EClcToolMode） */
UENUM(BlueprintType)
enum class EClcToolMode : uint8
{
	Opener     UMETA(DisplayName = "开窗器"),
	Flashlight UMETA(DisplayName = "手电筒")
};

class USphereComponent;
class UStaticMeshComponent;
class UCameraComponent;
class USpringArmComponent;
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
class CLAUDECORE_API AClcJadeWorkbench : public AActor
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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FText InteractionPrompt = FText::FromString(TEXT("Press [F] to use Workbench"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	float CameraDistance = 200.0f;

	/** 开窗材质路径（AClcOpeningStone 初始化时加载） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	FString OpeningMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneOpening.M_StoneOpening");

	/** WASD 旋转速度倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Config")
	float RotationInputScale = 1.0f;

	// ---- 工具蓝图槽位（指定 BP 子类来覆写参数 / Mesh / 表现） ----

	/** 开窗器工具类——默认用 C++ 类，可改为 BP 子类覆写 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Tools")
	TSubclassOf<AClcStoneTool> OpeningToolClass;

	/** 手电筒工具类——默认用 C++ 类，可改为 BP 子类覆写 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workbench|Tools")
	TSubclassOf<AClcStoneTool> FlashlightToolClass;

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

	// ---- 按键边沿检测（自维护，避免输入模式切换重置 WasInputKeyJustPressed） ----
	bool bExitKeyPrev = false;
	bool bBKeyPrev = false;
	bool bTKeyPrev = false;

	/** 背包开闭状态（轮询用，检测全局 IA_Backpack 触发的开关） */
	bool bBackpackWasOpen = false;

	// ---- 工作台状态 ----

	EClcWorkbenchState CurrentState = EClcWorkbenchState::Inactive;

	UPROPERTY()
	AClcOpeningStone* OpeningStone = nullptr;

	int32 ActiveStoneBackpackIndex = -1;
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
	void SetWorkbenchCursor(bool bVisible);

	// ---- 工具管理 ----

	void CycleToolMode();
	void SwitchToolMode(EClcToolMode NewMode);
	void SpawnCurrentTool();
	void DestroyCurrentTool();

	UFUNCTION()
	void OnBackpackStoneSelected(int32 StoneIndex);

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
