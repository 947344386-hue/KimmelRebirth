// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcEnergyBall.generated.h"

class USphereComponent;

/**
 * 鹰眼能量球——摊位上方浮动，大小正比于摊位含金量
 */
UCLASS()
class CLAUDECORE_API AClcEnergyBall : public AActor
{
	GENERATED_BODY()

public:
	AClcEnergyBall();

	/** 设置缩放值（1-4区间） */
	UFUNCTION(BlueprintCallable, Category = "ClcEnergyBall")
	void SetScaleValue(float Scale);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcEnergyBall")
	UStaticMeshComponent* BallMesh;

private:
	float CurrentScale = 1.0f;
};
