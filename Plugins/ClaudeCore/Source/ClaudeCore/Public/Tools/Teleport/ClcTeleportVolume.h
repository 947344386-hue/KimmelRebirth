// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClcTeleportVolume.generated.h"

class AClcTeleportPoint;
class UBoxComponent;
class UClcTeleportMenuWidget;
class ULocalPlayer;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class CLAUDECORE_API AClcTeleportVolume : public AActor
{
	GENERATED_BODY()

public:
	AClcTeleportVolume();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Teleport")
	FText GetMenuTitle() const { return MenuTitle; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	int32 GetActivationPriority() const { return ActivationPriority; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	TSubclassOf<UClcTeleportMenuWidget> GetMenuWidgetClass() const { return MenuWidgetClass; }

	UFUNCTION(BlueprintPure, Category = "Teleport")
	FText GetPromptText() const { return PromptText; }

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	TArray<AClcTeleportPoint*> GetValidDestinations() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	FText MenuTitle;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	TArray<TObjectPtr<AClcTeleportPoint>> Destinations;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	int32 ActivationPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teleport|UI")
	TSubclassOf<UClcTeleportMenuWidget> MenuWidgetClass;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport|UI")
	FText PromptText;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	ULocalPlayer* GetLocalPlayerForActor(AActor* Actor) const;
	void RegisterForPlayer(ULocalPlayer* LocalPlayer);
	void UnregisterForPlayer(ULocalPlayer* LocalPlayer);

	TArray<TWeakObjectPtr<ULocalPlayer>> RegisteredPlayers;

	/** 开局重叠补注册——玩家在体积内出生时 BeginOverlap 不触发 */
	void ScanInitialOverlap();
	FTimerHandle InitialOverlapTimer;
	int32 InitialOverlapAttempts = 0;
};
