// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "UI/ClcCuttingTableHUD.h"
#include "Interfaces/ClcInteractable.h"
#include "ClcCuttingTable.generated.h"

class AClcCuttingStone;
class APlayerController;
class APawn;
class IClcStoneCarrier;
class UBoxComponent;
class UCameraComponent;
class UClcBackpackSubsystem;
class UClcCuttingTableHUD;
class UClcInteractionIndicator;
class UProceduralMeshComponent;
class USceneComponent;
class USphereComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UDecalComponent;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class CLAUDECORE_API AClcCuttingTable : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcCuttingTable();

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingTable")
	bool IsInCuttingMode() const { return CurrentState != EClcCuttingTableState::Inactive; }

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingTable")
	bool HasActiveStone() const { return CurrentState == EClcCuttingTableState::StoneOnBench && CuttingStone != nullptr; }

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingTable")
	bool GetActiveStone(FClcStoneRuntimeData& OutData) const;

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingTable")
	AClcCuttingStone* GetCuttingStone() const { return CuttingStone; }

	UFUNCTION(BlueprintCallable, Category = "ClcCuttingTable")
	bool ExecuteBladeDrop();

	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> TableRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> StoneSpawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> BladePoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> WorkCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> LeftCutCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> RightCutCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CatchBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UClcInteractionIndicator> InteractionIndicator;

	/**
	 * 自适应补光——按当前状态/工位调节强度，避免太暗看不见石头细节。
	 * 位置/锥角/颜色等直接在 BP 的 FillLight 组件上调；这里只暴露强度档位。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> FillLight;

	/**
	 * 切刀瞄准线贴花——投影到石头表面显示一条沿切割平面的发光线 + 两端横标，
	 * 颜色按切块合理性四态动态变化（红/黄/绿/橙），与 HUD 同源。
	 * 挂在 BladePoint，随刀片位置移动；切割电影感期间隐藏。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> AimDecal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Interaction", meta = (ClampMin = "50.0"))
	float TriggerRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Interaction")
	FKey EnterKey = FKey("F");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Interaction")
	FKey ExitKey = FKey("Escape");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Interaction")
	FKey BackpackKey = FKey("B");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Controls")
	FKey MoveLeftKey = FKey("A");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Controls")
	FKey MoveRightKey = FKey("D");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Controls")
	FKey CutKey = FKey("SpaceBar");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Controls", meta = (ClampMin = "1.0"))
	float StoneMoveSpeed = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Controls", meta = (ClampMin = "0.0"))
	float CutEdgeMargin = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Blade", meta = (ClampMin = "1.0"))
	float BladeMaxDurability = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Blade", meta = (ClampMin = "0.01"))
	float BladeDurabilityPerCut = 10.0f;

	// ---- 切割动画循环（电影感下刀）----

	/** 升刀时长（刀片旋转向上升，秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.05"))
	float LiftDuration = 0.5f;

	/** 切割执行缓冲（秒；ExecuteCut 在此阶段开始时同步执行） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.01"))
	float CutDuration = 0.1f;

	/** 降刀时长（刀片下降回原位，秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.05"))
	float DescendDuration = 0.35f;

	/** 展示切割面时长（侧视停留，秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.1"))
	float ShowCutFaceDuration = 2.0f;

	/** 刀片上升高度（沿 BladePoint 局部 Z，cm） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "1.0"))
	float BladeLiftHeight = 80.0f;

	/** 刀片旋转速度（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "1.0"))
	float BladeSpinSpeed = 720.0f;

	/** 刀片自旋轴（局部 X，已按当前刀片 mesh 验证） */
	FVector BladeSpinAxis = FVector(1.0f, 0.0f, 0.0f);

	// ---- 切下块推出物理（按切片体积驱动 + 翻飞扰动） ----
	// 速度 = BaseSpeed + VolumeK * CutVolume，让大块"沉而有力"、小块"轻而快"，
	// 避免固定速度导致大块推不动、小块飞太猛。体积以 cm³ 计（CutAwayTotal * VoxelVolume）。

	/** 朝相机方向的基础水平速度（cm/s，体积无关分量，保证最小推力） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0"))
	float CutPieceBaseSpeed = 60.0f;

	/** 体积速度系数（cm/s per cm³，乘以切片体积）；大块额外加速，让大切片也能被崩出去 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0"))
	float CutPieceVolumeSpeedK = 0.8f;

	/** 体积参与速度计算的软上限（cm³，超过则不再线性加速，防极大片飞太快） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "1.0"))
	float CutPieceVolumeSpeedCap = 400.0f;

	/** 垂直上抛速度增量（cm/s，让切片腾空翻一下再落地，非纯平推） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0"))
	float CutPieceLiftSpeed = 120.0f;

	/** 水平方向随机扰动最大角度（度，0=严格朝相机；±范围内每片飞向不同） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float CutPieceYawJitterDeg = 18.0f;

	/** 朝相机倾倒的角速度增量（rad/s，体积无关基线） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0"))
	float CutPieceTipAngularSpeed = 1.5f;

	/** 切下块物理模拟后自动清理延迟（秒，0=不清理） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Cinematic", meta = (ClampMin = "0.0"))
	float CutPieceCleanupDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Voxel", meta = (ClampMin = "16", ClampMax = "96"))
	int32 VoxelResolution = 48;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Voxel", meta = (ClampMin = "0.01", ClampMax = "0.95"))
	float FallbackDefectCoverage = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Material")
	FString ShellMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneShell.M_StoneShell");

	/** 切刀瞄准线贴花材质（Decal）——含 Color/Opacity 参数供 MID 动态调色 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|AimDecal")
	FString AimDecalMaterialPath = TEXT("/Game/JadeBetting/Materials/M_CutAimLine.M_CutAimLine");

	/** 贴花投影起点高于石头 Bounds 顶部的距离（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|AimDecal", meta = (ClampMin = "0.0"))
	float AimDecalSurfaceOffset = 5.0f;

	/** 贴花平面相对石头水平 Bounds 的覆盖倍率 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|AimDecal", meta = (ClampMin = "1.0"))
	float AimDecalCoverageScale = 1.2f;

	/** 瞄准线粗细（UV 缩放，<1 更粗，>1 更细）——传给材质的 LineWidth 参数，BP 可实时调 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CuttingTable|AimDecal", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float AimDecalLineWidth = 1.0f;

	/** 图案绕向下投影轴的旋转修正（度）；用于匹配 BP 中旋转过的石头/刀具朝向 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|AimDecal", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float AimDecalRotationOffset = 90.0f;

	/** 向下投影深度在石头高度之外的额外余量（cm） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|AimDecal", meta = (ClampMin = "0.0"))
	float AimDecalProjectionPadding = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Interaction")
	FText InteractionPrompt = FText::FromString(TEXT("按 F 使用解石台"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|HUD")
	TSubclassOf<UClcCuttingTableHUD> HUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|HUD", meta = (ClampMin = "0.02"))
	float HUDPushInterval = 0.1f;

	// ---- 自适应补光强度档位（位置/锥角/颜色在 BP 的 FillLight 组件上调） ----

	/** 未进入解石台时的补光强度（0=灭） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|FillLight", meta = (ClampMin = "0.0"))
	float FillLightInactiveIntensity = 0.0f;

	/** 进入但还没放石头时的补光强度（看清台面） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|FillLight", meta = (ClampMin = "0.0"))
	float FillLightIdleIntensity = 2500.0f;

	/** 石头就位，正常解石补光强度（亮，看清切面细节） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|FillLight", meta = (ClampMin = "0.0"))
	float StoneOnBenchFillLightIntensity = 7000.0f;

	/** 补光强度过渡速度（越大越快，0=瞬切，建议 8~12） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|FillLight", meta = (ClampMin = "0.0"))
	float FillLightTransitionSpeed = 10.0f;

	UFUNCTION(BlueprintNativeEvent, Category = "CuttingTable|Events")
	void OnEnterCuttingMode();

	UFUNCTION(BlueprintNativeEvent, Category = "CuttingTable|Events")
	void OnExitCuttingMode();

	UFUNCTION(BlueprintNativeEvent, Category = "CuttingTable|Events")
	void OnStonePlaced(const FClcStoneInternalData& StoneData);

	UFUNCTION(BlueprintNativeEvent, Category = "CuttingTable|Events")
	void OnStoneRemoved();

	UFUNCTION(BlueprintNativeEvent, Category = "CuttingTable|Events")
	void OnBladeCut(int32 CutAwayTotal, int32 CutAwayJade, int32 CutAwayCrack);

private:
	enum class EClcCuttingTableState : uint8
	{
		Inactive,
		AwaitingStone,
		StoneOnBench,
		CuttingCinematic
	};

	enum class EBladePhase : uint8
	{
		Idle,
		LiftBlade,
		Cut,
		DescendBlade,
		ShowCutFace,
		Finish
	};

	EClcCuttingTableState CurrentState = EClcCuttingTableState::Inactive;

	UPROPERTY()
	TObjectPtr<AClcCuttingStone> CuttingStone;

	UPROPERTY()
	TObjectPtr<UClcCuttingTableHUD> HUDWidget;

	FClcStoneRuntimeData ActiveStoneData;
	TWeakObjectPtr<APawn> PlayerInRange;
	TWeakObjectPtr<APlayerController> CachedPC;
	TWeakObjectPtr<UObject> CachedCarrierObj;
	IClcStoneCarrier* CachedCarrier = nullptr;
	FVector StoneBaseWorldLocation = FVector::ZeroVector;
	FVector MovementAxisWorld = FVector::ForwardVector;
	float StoneOffset = 0.0f;
	float MovementRange = 0.0f;
	float HUDPushTimer = 0.0f;

	bool bExitKeyPrev = false;
	bool bBackpackKeyPrev = false;
	bool bCutKeyPrev = false;
	bool bPawnInputDisabled = false;
	double LastEnterToastTime = 0.0;

	// ---- 切割动画循环状态 ----
	EBladePhase BladePhase = EBladePhase::Idle;
	float PhaseTimer = 0.0f;
	FVector BladePointInitialRelativeLocation = FVector::ZeroVector;
	bool bCutExecutedThisCycle = false;
	bool bCutRemovedNegativeSide = false;
	/** 循环开始时缓存的刀口平面（预判/相机/实际切割共用，防升刀偏移） */
	FVector PendingCutPlanePoint = FVector::ZeroVector;
	FVector PendingCutPlaneNormal = FVector::ZeroVector;
	/** 上一刀切割结果缓存（ShowCutFace 阶段飘字用） */
	int32 LastCutAwayTotal = 0;
	int32 LastCutAwayJade = 0;
	int32 LastCutAwayCrack = 0;
	int32 LastCutAwayImpurity = 0;
	int32 LastPieceGold = 0;
	float LastCutVolume = 0.0f;
	TWeakObjectPtr<UCameraComponent> ActiveCutCamera;
	TWeakObjectPtr<UProceduralMeshComponent> LastCutAwayPiece;
	FTimerHandle CutPieceCleanupHandle;

	/** 补光当前强度（每帧平滑追向 Target） */
	float CurrentFillLightIntensity = 0.0f;
	/** 补光目标强度（由 UpdateFillLightTarget 按状态算出） */
	float TargetFillLightIntensity = 0.0f;

	/** 切刀瞄准线贴花 MID——运行时调 Color/Opacity 按四态配色 */
	UPROPERTY()
	UMaterialInstanceDynamic* AimDecalMID = nullptr;
	/** 上次配色状态缓存，避免每帧重复 SetVectorParameterValue */
	uint8 LastAimDecalState = 255;

	void CachePlayerRefs();
	void EnterCuttingMode();
	void ExitCuttingMode();
	void ProcessCuttingInput(float DeltaTime);
	void HandleBackpackInput();

	// ---- 切割动画循环 ----
	bool StartCutCinematic();
	void TickCutCinematic(float DeltaTime);
	void EnterBladePhase(EBladePhase NewPhase);
	void ExecuteCutDuringCinematic();
	void ShowCutFaceInfo();
	void FinishCutCinematic();
	void CleanupLastCutPiece();
	bool PlaceStoneOnBench(int32 StoneIndex);
	void RemoveStoneFromBench();
	void DestroyCuttingStone();
	void BindToBackpackWidget();
	void UnbindFromBackpackWidget();
	void SetCuttingInputMode();
	bool IsStoneEligible(const FClcStoneRuntimeData& StoneData) const;
	bool HasEligibleStone() const;
	float ResolveTargetCoverage(const FClcStoneRuntimeData& StoneData) const;
	bool CanCutNow() const;
	void CreateHUD();
	void DestroyHUD();
	void PushHUDData();
	EClcCutSizeState ComputeCutSizeState(float& OutRatio) const;
	UClcBackpackSubsystem* GetBackpack() const;

	UFUNCTION()
	void OnBackpackStoneSelected(int32 StoneIndex);

	UFUNCTION()
	bool QueryCanSelect();

	// 以下成员已废弃，保留声明以兼容现有调用（实际逻辑已迁移到 UClcInteractionComponent）
	bool IsLookedAtByPlayer() const;

	// ---- 自适应补光 ----

	/** 按当前状态重算目标强度 */
	void UpdateFillLightTarget();
	/** 每帧把当前强度平滑追向目标并应用到 FillLight */
	void TickFillLight(float DeltaTime);
	/** 按石头实时 Bounds 把 AimDecal 放到顶部，并沿世界向下投影 */
	bool UpdateAimDecalTransform();
	/** 按切块合理性四态刷新 AimDecal 颜色/透明度（与 HUD 同源） */
	void UpdateAimDecalColor(EClcCutSizeState State);

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
