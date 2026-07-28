// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcTeleportPoint.generated.h"

class UArrowComponent;
class UDrawSphereComponent;
class USceneComponent;

UCLASS(Blueprintable)
class CLAUDECORE_API AClcTeleportPoint : public AActor
{
	GENERATED_BODY()

public:
	AClcTeleportPoint();

	UFUNCTION(BlueprintPure, Category = "Teleport")
	FText GetDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	float GetArrivalClearance() const { return ArrivalClearance; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport|Editor")
	TObjectPtr<UDrawSphereComponent> SelectionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport|Editor")
	TObjectPtr<UArrowComponent> ArrivalArrow;
#endif

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	FText DisplayName;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	bool bEnabled = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = "0.0"))
	float ArrivalClearance = 2.0f;
};
