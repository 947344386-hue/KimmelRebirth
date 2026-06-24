// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcStoneMeshConfig.generated.h"

UCLASS(BlueprintType)
class CLAUDECORE_API UClcStoneMeshConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes") TArray<TSoftObjectPtr<UStaticMesh>> StoneMeshes;
	UFUNCTION(BlueprintCallable, Category = "ClcStoneMesh") UStaticMesh* GetRandomMesh() const;
};