// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ClcTeleportSubsystem.generated.h"

class AClcTeleportPoint;
class AClcTeleportVolume;
class UClcTeleportMenuWidget;
class UEnhancedInputComponent;
class UInputAction;

UCLASS()
class CLAUDECORE_API UClcTeleportSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterVolume(AClcTeleportVolume* Volume);
	void UnregisterVolume(AClcTeleportVolume* Volume);

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	bool OpenMenu();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, Category = "Teleport")
	bool RequestTeleport(AClcTeleportPoint* Destination);

	UFUNCTION(BlueprintPure, Category = "Teleport")
	bool IsMenuOpen() const { return bMenuOpen; }

private:
	struct FActiveVolume
	{
		TWeakObjectPtr<AClcTeleportVolume> Volume;
		uint64 EnterSequence = 0;
	};

	APlayerController* GetPlayerController() const;
	bool CanOpenMenu() const;
	void RecomputeCurrentVolume();
	void EnsureInputBinding();
	void RemoveInputBinding();
	void ShowBlockedMessage() const;

	TArray<FActiveVolume> ActiveVolumes;
	TWeakObjectPtr<AClcTeleportVolume> CurrentVolume;
	TWeakObjectPtr<AClcTeleportVolume> OpenedFromVolume;
	uint64 NextEnterSequence = 0;

	UPROPERTY(Transient)
	TObjectPtr<UClcTeleportMenuWidget> MenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TeleportMenuAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	uint32 InputBindingHandle = 0;
	bool bMenuOpen = false;
	bool bOwnsInputState = false;

	/** 按键提示句柄：CurrentVolume 有效时注册 T，离开/Deinitialize 注销 */
	int32 TeleportPromptHandle = 0;
};
