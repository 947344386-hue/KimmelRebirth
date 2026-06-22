#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JBJadeInteractionComponent.generated.h"

class AJBJadeStone;
class UCameraComponent;

UCLASS(ClassGroup = (JB), Blueprintable, meta = (BlueprintSpawnableComponent))
class CLAUDECORE_API UJBJadeInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJBJadeInteractionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Target")
	TObjectPtr<AJBJadeStone> TargetStone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Camera")
	TObjectPtr<UCameraComponent> TargetCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Paint")
	float BrushRadius = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Paint")
	float PaintStepThreshold = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Zoom")
	float ZoomFOV = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Zoom")
	float NormalFOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JB|Zoom")
	float ZoomInterpSpeed = 8.0f;

	UFUNCTION(BlueprintCallable, Category = "JB|State")
	void SetEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|State")
	bool IsEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintCallable, Category = "JB|Input")
	void StartPaint();

	UFUNCTION(BlueprintCallable, Category = "JB|Input")
	void StopPaint();

	UFUNCTION(BlueprintCallable, Category = "JB|Input")
	void StartZoom();

	UFUNCTION(BlueprintCallable, Category = "JB|Input")
	void StopZoom();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|State")
	bool IsPainting() const { return bIsPainting; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JB|State")
	bool IsZooming() const { return bIsZooming; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void PaintAtCursor();
	bool TraceCrust(FVector2D& OutUV) const;
	void UpdateCameraZoom(float DeltaTime);

	bool bInteractionEnabled = true;
	bool bIsPainting = false;
	bool bIsZooming = false;
	FVector2D LastPaintUV = FVector2D(-1.0f, -1.0f);
};