// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcFacilityManager.generated.h"

class UArrowComponent;
class AClcJadeWorkbench;
class AClcCuttingTable;

/**
 * 设施中继管理器——放在关卡中，管理擦石台/解石台的生成、布局和升级响应。
 *
 * 两套布局：
 *   Solo（仅擦石台）→ 擦石台在 SoloWorkbenchPoint
 *   Paired（已购解石台）→ 擦石台在 PairedWorkbenchPoint，解石台在 PairedCuttingTablePoint
 *
 * 编辑器预览工作流：
 *   1. 拖 BP_FacilityManager 入关卡
 *   2. Details 面板点「Preview Solo」→ 擦石台预览体出现，拖到想要的位置
 *   3. 再点「Preview Solo」→ 预览体位置保存回 SoloWorkbenchPoint，预览体销毁
 *   4. 同样点「Preview Paired」摆双台布局
 *   5. 预览结束后清掉关卡里残留的预览体（选 Stop Preview）
 */
UCLASS(Blueprintable)
class CLAUDECORE_API AClcFacilityManager : public AActor
{
	GENERATED_BODY()

public:
	AClcFacilityManager();

	/** 重新检查升级状态并刷新布局——升级站购买成功后调用 */
	UFUNCTION(BlueprintCallable, Category = "ClcFacility")
	void RefreshLayout();

	UFUNCTION(BlueprintCallable, Category = "ClcFacility")
	AClcJadeWorkbench* GetWorkbench() const { return SpawnedWorkbench; }

	UFUNCTION(BlueprintCallable, Category = "ClcFacility")
	AClcCuttingTable* GetCuttingTable() const { return SpawnedCuttingTable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	// ---- 根组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> ManagerRoot;

	// ---- Solo 布局（仅擦石台） ----

	/** Solo：擦石台位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> SoloWorkbenchPoint;

	// ---- Paired 布局（擦石台 + 解石台） ----

	/** Paired：擦石台位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> PairedWorkbenchPoint;

	/** Paired：解石台位置 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> PairedCuttingTablePoint;

	// ---- 蓝图类槽位 ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility|Classes")
	TSubclassOf<AClcJadeWorkbench> WorkbenchClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility|Classes")
	TSubclassOf<AClcCuttingTable> CuttingTableClass;

#if WITH_EDITOR
	// ---- 编辑器预览（CallInEditor 按钮） ----

	/** 预览 Solo 布局：在 SoloWorkbenchPoint 处生成擦石台预览体，拖到目标位置后再点一次保存 */
	UFUNCTION(CallInEditor, Category = "Facility|Preview")
	void ToggleSoloPreview();

	/** 预览 Paired 布局：同时生成擦石台和解石台预览体，拖到目标位置后再点一次保存 */
	UFUNCTION(CallInEditor, Category = "Facility|Preview")
	void TogglePairedPreview();

	/** 强制停止所有预览并丢弃未保存的位置 */
	UFUNCTION(CallInEditor, Category = "Facility|Preview")
	void StopPreview();
#endif

private:
	UPROPERTY()
	TObjectPtr<AClcJadeWorkbench> SpawnedWorkbench;

	UPROPERTY()
	TObjectPtr<AClcCuttingTable> SpawnedCuttingTable;

	bool bPairedLayout = false;

	void ApplySoloLayout();
	void ApplyPairedLayout();
	void DestroyWorkbench();
	void DestroyCuttingTable();

#if WITH_EDITOR
	enum class EPreviewMode : uint8 { None, Solo, Paired };
	EPreviewMode PreviewMode = EPreviewMode::None;

	TObjectPtr<AActor> PreviewWorkbench;
	TObjectPtr<AActor> PreviewCuttingTable;

	void SpawnPreviewActors(EPreviewMode Mode);
	void SavePreviewTransforms();
	void DestroyPreviewActors();
	void SetArrowFromActor(UArrowComponent* Arrow, const AActor* Source);
#endif
};