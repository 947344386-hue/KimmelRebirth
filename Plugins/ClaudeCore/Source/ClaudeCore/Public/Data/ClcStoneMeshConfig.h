// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcStoneMeshConfig.generated.h"

/**
 * 石头模型配置——设计师在编辑器中增减引用，无需碰代码
 * 摊位生成石头时从数组中随机选择模型
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcStoneMeshConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 可用石头模型列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TArray<TSoftObjectPtr<UStaticMesh>> StoneMeshes;

	/** 获取随机模型（带权重的版本可后续扩展） */
	UFUNCTION(BlueprintCallable, Category = "ClcStoneMesh")
	UStaticMesh* GetRandomMesh() const;
};
