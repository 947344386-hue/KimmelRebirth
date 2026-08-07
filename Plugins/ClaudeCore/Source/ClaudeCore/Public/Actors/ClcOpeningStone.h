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
 * 工作台上正在被擦石的石头——只管 3D 表现、材质、遮罩、旋转、存档。
 * 擦石/手电逻辑由各自的 Tool 类（AClcOpeningTool / AClcFlashlightTool）驱动。
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
	 *  MaterialAssetPath 指定擦石材质路径（如 /Game/JadeBetting/Materials/M_StoneOpening）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool Initialize(const FClcStoneRuntimeData& StoneData, const FString& MaterialAssetPath);

	// ---- 旋转 ----

	/** 累加旋转输入（由工作台 Tick 转发 WASD）。
	 *  CameraRight/CameraUp 为工作台 WorkCamera 的右/上向量，
	 *  确保旋转始终以屏幕为基准：W/S 绕相机 Y 轴（左右），A/D 绕相机 X 轴（上下）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	void AddRotationInput(float DeltaPitch, float DeltaYaw, const FVector& CameraRight, const FVector& CameraUp);

	/** 平滑旋转回初始朝向，返回 true 表示已到位。
	 *  由工作台在 R 键按下后每帧调用，直到返回 true。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool ResetRotation(float DeltaTime, float InterpSpeed);

	// ---- 打磨（委托——由 AClcOpeningTool 调用） ----

	/** 在指定 UV 位置执行打磨——更新遮罩 + 累计统计。 */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool GrindAtUV(float U, float V);

	// ---- 存档 ----

	/** 获取当前擦石进度，用于退出时写回 FClcStoneRuntimeData */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	void GetOpeningProgress(float& OutOpenedRatio, float& OutOpenedGreenRatio, float& OutOpenedBlackRatio,
		float& OutOpenedImpurityRatio, float& OutOpenedCrackRatio) const;

	/** 获取当前石头的运行时数据（含已更新的擦石信息） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool GetStoneData(FClcStoneRuntimeData& OutData) const;

	/** 标记石头已讨价还价结算，锁定最终售价（写回内部数据；锁定后禁止擦石/再讨价） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	void MarkHaggleResolved(int32 LockedPrice);

	/** 该石头是否已讨价还价锁定（擦石/再讨价时据此门禁） */
	UFUNCTION(BlueprintCallable, Category = "ClcOpeningStone")
	bool IsHaggleResolved() const;

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

	/** 皮壳纹理配置 DataAsset 路径——空则走 DeveloperSettings 全局配置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config", meta = (MetaClass = "ClcShellTextureConfig"))
	FString ShellTextureConfigPath;

	/** 玉石纹理配置 DataAsset 路径——空则走 DeveloperSettings 全局配置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpeningStone|Config", meta = (MetaClass = "ClcJadeTextureConfig"))
	FString JadeTextureConfigPath;

private:
	/** 当前石头数据（运行时持续更新擦石进度） */
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

	/** 累计擦石面积（UV 比例 × 表面积） */
	float AccumulatedOpenedRatio = 0.0f;

	/** 累计暴露的玉肉面积比例 */
	float AccumulatedGreenRatio = 0.0f;

	/** 累计暴露的杂质面积比例 */
	float AccumulatedImpurityRatio = 0.0f;

	/** 累计暴露的裂纹面积比例 */
	float AccumulatedCrackRatio = 0.0f;

	/** 累计暴露的黑色（杂质+裂纹）面积比例——兼容旧字段 */
	float AccumulatedBlackRatio = 0.0f;

	/** 石头放置时的初始旋转（Mesh 世界四元数，用于 ResetRotation） */
	FQuat InitialMeshRotation;
};
