#pragma once

#include "CoreMinimal.h"
#include "JBJadeTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct CLAUDECORE_API FJBJadeStoneConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Mesh")
	TObjectPtr<UStaticMesh> BaseMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Layers")
	float CrustScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Layers")
	float CoreScale = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Materials")
	TObjectPtr<UMaterialInterface> CrustMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Materials")
	TObjectPtr<UMaterialInterface> CoreMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Mask", AdvancedDisplay)
	int32 MaskResolution = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Texture", AdvancedDisplay)
	int32 NoiseTextureSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Texture", AdvancedDisplay)
	float NoiseScale = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Texture", AdvancedDisplay)
	int32 NoiseOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Texture", AdvancedDisplay)
	int32 NoiseSeed = 1042;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Texture", AdvancedDisplay)
	float NoiseContrast = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Rotation")
	float RotationSpeed = 90.0f;
};