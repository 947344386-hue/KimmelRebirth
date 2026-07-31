// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ClcJadeTypes.h"
#include "ClcOpeningMaskComponent.generated.h"

class UTexture2D;
class UTextureRenderTarget2D;

/** 打磨一次返回的结果——用于更新 FClcStoneRuntimeData */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStoneOpeningResult
{
	GENERATED_BODY()

	/** 本次笔刷覆盖的总 UV 面积占比 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	float AreaFraction = 0.0f;

	/** 本次是否暴露了新的玉肉像素 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	bool bHitGreen = false;

	/** 本次是否暴露了新的杂质像素 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	bool bHitImpurity = false;

	/** 本次是否暴露了新的裂纹像素 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	bool bHitCrack = false;

	/** 本次新暴露的玉肉面积占比 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	float NewGreenFraction = 0.0f;

	/** 本次新暴露的杂质面积占比 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	float NewImpurityFraction = 0.0f;

	/** 本次新暴露的裂纹面积占比 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	float NewCrackFraction = 0.0f;

	/** 本次新暴露的杂裂（杂质+裂纹）合计占比——兼容旧字段 */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	float NewBlackFraction = 0.0f;

	/** 旧字段：本次是否暴露新的杂裂（杂质或裂纹） */
	UPROPERTY(BlueprintReadOnly, Category = "Opening")
	bool bHitBlack = false;
};

/**
 * 开窗遮罩组件——管理 RenderTarget 遮罩（皮壳可见性）和揭示纹理（底层颜色）。
 * 挂在工作台 Actor 上，每块石头初始化一次。
 */
UCLASS(ClassGroup = (Clc), meta = (BlueprintSpawnableComponent))
class CLAUDECORE_API UClcOpeningMaskComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClcOpeningMaskComponent();

	virtual void BeginDestroy() override;

	// ---- 配置 ----

	/** 遮罩 RT 的分辨率（与 DistributionMap 一致） */
	static constexpr int32 MaskResolution = 256;

	/** 打磨笔刷半径（UV 空间，0~1） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening|Config")
	float BrushRadius = 0.04f;

	/** 打磨笔刷硬度（0=柔和渐边, 1=硬边圆） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening|Config")
	float BrushHardness = 0.3f;

	// ---- 初始化 ----

	/** 用石头数据初始化遮罩和底层纹理（RT 全黑=皮壳全覆盖，RevealTex 按分布图着色） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void InitializeFromStoneData(const FClcStoneInternalData& StoneData);

	/** 清除遮罩——所有像素归零，皮壳恢复全覆盖 */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void ResetMask();

	/** 将当前遮罩缓冲区写入 RuntimeData（退出时调用） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void SaveMaskToData(FClcStoneRuntimeData& OutData) const;

	/** 从 RuntimeData 中的存档缓冲区恢复遮罩（再进入时调用，替代 ResetMask） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void RestoreMaskFromData(const FClcStoneRuntimeData& InData);

	/** 将材质参数（MaskRT、RevealTex）推送到指定的动态材质实例 */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void ApplyToMaterial(UMaterialInstanceDynamic* DynMaterial);

	// ---- 打磨 ----

	/**
	 * 在 UV 坐标处打磨——在遮罩 RT 上画一个圆形笔刷。
	 * 返回此笔刷位置暴露的材质类型统计（0=皮壳 1=绿玉 2=杂裂），用于更新 RuntimeData。
	 */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	FClcStoneOpeningResult GrindAtUV(float UV_U, float UV_V);

	/** 获取当前遮罩 RT（供材质使用） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	UTextureRenderTarget2D* GetMaskRT() const { return MaskRT; }

	/** 获取揭示纹理（供材质使用） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	UTexture2D* GetRevealTexture() const { return RevealTex; }

	/** 获取类型纹理（R=玉mask, G=杂mask，供材质按类型混合玉/杂 PBR 纹理） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	UTexture2D* GetTypeTexture() const { return TypeTex; }

	/** 获取累计开窗比例（0~1） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	float GetOpenedRatio() const;

	/** 获取已暴露绿色面积比例（0~1，相对总像素）——重载恢复后也有效 */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	float GetExposedGreenRatio() const;

	/** 获取已暴露杂质面积比例（0~1，相对总像素） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	float GetExposedImpurityRatio() const;

	/** 获取已暴露裂纹面积比例（0~1，相对总像素） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	float GetExposedCrackRatio() const;

	/** 计算当前已开窗绿色像素的最大连通域像素数（4连通 BFS，每 0.3s 调一次性能无忧） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	int32 ComputeLargestGreenConnectedComponent() const;

private:
	void EnsureMaskRT();
	void EnsureRevealTexFromDistribution(const FClcStoneDistributionMap& Distribution, int32 Seed, EClcJadeGrade Grade);

	/** 把分布图编码成 TypeTex（R=玉mask, G=杂mask），供材质按类型混合玉/杂纹理 */
	void EnsureTypeTexFromDistribution(const FClcStoneDistributionMap& Distribution);

	/** 按石头 Seed 注入调制参数（UV旋转/色调/种水）到 MID */
	void ApplyModulationParams(UMaterialInstanceDynamic* MID);

	/** 将 CPU 缓冲区上传到 GPU RT */
	void UploadMaskToGPU();

	// ---- CPU 缓冲区 ----

	TArray<uint8> MaskBuffer; // 0~255，MaskResolution*MaskResolution

	/** 已开窗像素数（>=128 阈值），增量维护避免每次 GetOpenedRatio 全量遍历 */
	int32 OpenedPixelCount = 0;

	/** 已开窗像素中绿色（玉肉）像素数——与 OpenedPixelCount 对称，GrindAtUV 增量、RestoreMaskFromData 全量重算 */
	int32 OpenedGreenPixelCount = 0;

	/** 已开窗像素中杂质像素数——同上 */
	int32 OpenedImpurityPixelCount = 0;

	/** 已开窗像素中裂纹像素数——同上 */
	int32 OpenedCrackPixelCount = 0;

	/** 已开窗像素中杂裂（杂质+裂纹）合计像素数——兼容旧字段 */
	int32 OpenedBlackPixelCount = 0;

	// ---- GPU 资源 ----

	UPROPERTY()
	UTextureRenderTarget2D* MaskRT;

	UPROPERTY()
	UTexture2D* RevealTex;

	/** 类型纹理：R=玉mask(1=玉), G=杂mask(1=杂)。纹理主导混合用 */
	UPROPERTY()
	UTexture2D* TypeTex;

	UPROPERTY()
	FClcStoneDistributionMap CachedDistribution;

	int32 CachedSeed = 0;
	EClcJadeGrade CachedGrade = EClcJadeGrade::Bean;
};
