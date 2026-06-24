// Copyright ClaudeCore. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStone.generated.h"
class UStaticMeshComponent; class UClcInteractionIndicator;
UCLASS()
class CLAUDECORE_API AClcStone : public AActor, public IClcInteractable
{
	GENERATED_BODY()
public:
	AClcStone();
	UFUNCTION(BlueprintCallable) void Initialize(const FClcStoneInternalData& InData, UStaticMesh* InMesh, float InScale, const FString& InDisplayName);
	UFUNCTION(BlueprintCallable) void SetDisplayName(const FString& NewName) { RuntimeData.DisplayName = NewName; }
	UFUNCTION(BlueprintCallable) const FClcStoneRuntimeData& GetStoneData() const { return RuntimeData; }
	UFUNCTION(BlueprintCallable) void RecalculateSurfaceArea();
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;
	UFUNCTION(BlueprintCallable) bool PurchaseStone(AActor* Buyer);
	UFUNCTION(BlueprintCallable) void ShowInfoCard();
	UFUNCTION(BlueprintCallable) void HideInfoCard();
	void RemoveFromStall();
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UStaticMeshComponent* StoneMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UClcInteractionIndicator* InteractionIndicator;
private:
	FClcStoneRuntimeData RuntimeData;
	bool bPlayerInRange=false; bool bCameraAiming=false; bool bInfoCardVisible=false; float RangeCheckTimer=0;
	UPROPERTY() class UClcStoneInfoWidget* InfoCardWidget;
	UPROPERTY() TSubclassOf<UClcStoneInfoWidget> InfoCardClass;
};