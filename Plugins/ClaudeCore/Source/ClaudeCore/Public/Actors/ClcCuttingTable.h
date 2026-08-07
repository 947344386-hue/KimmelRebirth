// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "Interfaces/ClcInteractable.h"
#include "ClcCuttingTable.generated.h"

class AClcCuttingStone;
class APlayerController;
class APawn;
class IClcStoneCarrier;
class UCameraComponent;
class UClcBackpackSubsystem;
class UClcCuttingTableHUD;
class UClcInteractionIndicator;
class USceneComponent;
class USphereComponent;
class USpotLightComponent;
class USpringArmComponent;
class UStaticMeshComponent;

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
	TObjectPtr<UClcInteractionIndicator> InteractionIndicator;

	/**
	 * 自适应补光——按当前状态/工位调节强度，避免太暗看不见石头细节。
	 * 位置/锥角/颜色等直接在 BP 的 FillLight 组件上调；这里只暴露强度档位。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> FillLight;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Voxel", meta = (ClampMin = "16", ClampMax = "96"))
	int32 VoxelResolution = 48;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Voxel", meta = (ClampMin = "0.01", ClampMax = "0.95"))
	float FallbackDefectCoverage = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CuttingTable|Material")
	FString ShellMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneShell.M_StoneShell");

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
		StoneOnBench
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

	/** 补光当前强度（每帧平滑追向 Target） */
	float CurrentFillLightIntensity = 0.0f;
	/** 补光目标强度（由 UpdateFillLightTarget 按状态算出） */
	float TargetFillLightIntensity = 0.0f;

	void CachePlayerRefs();
	void EnterCuttingMode();
	void ExitCuttingMode();
	void ProcessCuttingInput(float DeltaTime);
	void HandleBackpackInput();
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

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
