// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcEnergyBall.generated.h"
UCLASS()
class CLAUDECORE_API AClcEnergyBall : public AActor
{
	GENERATED_BODY()
public:
	AClcEnergyBall();
	UFUNCTION(BlueprintCallable) void SetScaleValue(float Scale);
protected: virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* BallMesh;
private: float CurrentScale=1.0f;
};