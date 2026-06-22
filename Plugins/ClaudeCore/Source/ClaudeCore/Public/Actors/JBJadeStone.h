#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/JBJadeTypes.h"
#include "JBJadeStone.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class UTexture2D;

UCLASS(Blueprintable, BlueprintType)
class CLAUDECORE_API AJBJadeStone : public AActor
{
	GENERATED_BODY()

public:
	AJBJadeStone();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Config")
	FJBJadeStoneConfig StoneConfig;

	UFUNCTION(BlueprintCallable, Category = "JB|Stone")
	void RotateStone(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|Stone")
	UTextureRenderTarget2D* GetCrustMaskRT() const { return CrustMaskRT; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|Stone")
	UTexture2D* GetNoiseTexture() const { return NoiseTexture; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|Stone")
	float GetOpenedRatio() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|Stone")
	UStaticMeshComponent* GetCrustMesh() const { return CrustSM; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|Stone")
	UStaticMeshComponent* GetCoreMesh() const { return CoreSM; }

	void PaintMaskAtUV(FVector2D UV, float BrushRadius);

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(BlueprintReadOnly, Category = "JB|Components")
	TObjectPtr<UStaticMeshComponent> CrustSM;

	UPROPERTY(BlueprintReadOnly, Category = "JB|Components")
	TObjectPtr<UStaticMeshComponent> CoreSM;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> CrustMaskRT;
	UPROPERTY()
	TObjectPtr<UTexture2D> NoiseTexture;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CrustMI;
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CoreMI;

	TArray<FColor> MaskAccumulator;
	bool bMaskDirty = true;
	float CachedOpenedRatio = 0.0f;

private:
	void ApplyMeshAndScales();
	void EnsureRuntimeResources();
	void ApplyMaterials();
	void UploadMaskToRT();

	static float Hash2D(int32 X, int32 Y, int32 Seed);
	static float SmoothNoise2D(int32 X, int32 Y, int32 Seed);
	static float InterpolatedNoise2D(float X, float Y, int32 Seed);
	static float PerlinNoise2D(float X, float Y, int32 Seed, int32 Octaves, float Persistence);
};