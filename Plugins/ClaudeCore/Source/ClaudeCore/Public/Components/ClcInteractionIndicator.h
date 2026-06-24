// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ClcInteractionIndicator.generated.h"
class UClcInteractionWidget; class UWidgetComponent;
UCLASS(ClassGroup=(Clc), meta=(BlueprintSpawnableComponent))
class CLAUDECORE_API UClcInteractionIndicator : public UActorComponent
{
	GENERATED_BODY()
public:
	UClcInteractionIndicator();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InteractionRadius = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WidgetOffset = FVector(0,0,50);
	UFUNCTION(BlueprintCallable) int32 GetInteractionState() const { return CurrentState; }
protected: virtual void BeginPlay() override;
private:
	void UpdateInteractionState();
	UPROPERTY() UWidgetComponent* WidgetComp;
	UPROPERTY() UClcInteractionWidget* InteractionWidget;
	int32 CurrentState = 0;
	UPROPERTY() TSubclassOf<UClcInteractionWidget> WidgetClass;
};