// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ClcJadeTypes.h"
#include "ClcOpeningStone.generated.h"

class UStaticMeshComponent;
class UClcOpeningMaskComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 工作台上正在被开窗的石头——只管 3D 表现、材质、遮罩、旋转、存档。
 * 开窗/手电逻辑由各自的 Tool 类（AClcOpeningTool / AClcFlashlightTool）驱动。
 * 由 AClcJadeWorkbench 在 PlaceStoneOnBench 时 Spawn，RemoveStoneFromBench 时 Destroy。
 */
UCLASS()
class CLAUDECORE_API AClcOpeningStone : public AActor
{
	GENERATED_BODY()

public:
	AClcOpeningStone();

	// ---- 生命周期 ----

	/** 用石头运行时数据初始化——加载 Mesh、创建 MID、初始化遮罩、设材质。
	 *  MaterialAssetPath 指定开窗材质路径（如 /Game/JadeBetting/Materials/M_StoneOpening）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool Initialize(const FClcStoneRuntimeData& StoneData, const FString& MaterialAssetPath);

	// ---- 旋转 ----

	/** 累加旋转输入（由工作台 Tick 转发 WASD） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	void AddRotationInput(float DeltaPitch, float DeltaYaw);

	// ---- 打磨（委托——由 AClcOpeningTool 调用） ----

	/** 在指定 UV 位置执行打磨——更新遮罩 + 累计统计。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool GrindAtUV(float U, float V);

	// ---- 存档 ----

	/** 获取当前开窗进度，用于退出时写回 FClcStoneRuntimeData */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	void GetOpeningProgress(float& OutOpenedRatio, float& OutOpenedGreenRatio, float& OutOpenedBlackRatio) const;

	/** 获取当前石头的运行时数据（含已更新的开窗信息） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool GetStoneData(FClcStoneRuntimeData& OutData) const;

	// ---- 查询（供 Tool 类使用） ----

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	UStaticMeshComponent* GetStoneMesh() const { return StoneMesh; }

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	UClcOpeningMaskComponent* GetOpeningMask() const { return OpeningMaskComp; }

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	UMaterialInstanceDynamic* GetStoneMID() const { return StoneMID; }

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	ECollisionChannel GetTraceChannel() const { return TraceChannel.GetValue(); }

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	float GetRotationSpeed() const { return RotationSpeed; }

	/** 种水是否已暴露（HUD 用） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool IsGradeRevealed() const { return bGradeRevealed; }

	/** 通知 HUD 立即刷新（GrindAtUV 后由 Workbench 查询并重置） */
	bool ConsumeHUDDirty() { bool V = bHUDDirty; bHUDDirty = false; return V; }

protected:
	virtual void BeginPlay() override;

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcOpeningMaskComponent* OpeningMaskComp;

	// ---- 配置 ----

	/** WASD 旋转速度（度/秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config")
	float RotationSpeed = 90.0f;

	/** LineTrace 通道（默认 Visibility） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** 皮壳纹理配置 DataAsset 路径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config", meta = (MetaClass = "ClcShellTextureConfig"))
	FString ShellTextureConfigPath = TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig");

	/** 玉石纹理配置 DataAsset 路径 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config", meta = (MetaClass = "ClcJadeTextureConfig"))
	FString JadeTextureConfigPath = TEXT("/Game/JadeBetting/Data/DA_JadeTextureConfig");

private:
	/** 当前石头数据（运行时持续更新开窗进度） */
	FClcStoneRuntimeData CachedStoneData;

	/** 动态材质实例 */
	UPROPERTY()
	UMaterialInstanceDynamic* StoneMID;

	/** 是否已初始化 */
	bool bInitialized = false;

	/** 首次开到绿玉——种水暴露 */
	bool bGradeRevealed = false;

	/** HUD 需要立即刷新（GrindAtUV 触发） */
	bool bHUDDirty = false;

	/** 累计开窗面积（UV 比例 × 表面积） */
	float AccumulatedOpenedRatio = 0.0f;

	/** 累计暴露的绿色面积比例 */
	float AccumulatedGreenRatio = 0.0f;

	/** 累计暴露的黑色面积比例 */
	float AccumulatedBlackRatio = 0.0f;
};
