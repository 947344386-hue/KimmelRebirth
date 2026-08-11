// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ClcPauseMenuSubsystem.generated.h"

class UClcPauseMenuWidget;
class UInputAction;
class UEnhancedInputComponent;

/**
 * 暂停菜单子系统 —— 游戏中按 Esc 弹出。
 * 完全参照 UClcTeleportSubsystem：EnhancedInput + InputAction 绑定 Esc。
 */
UCLASS()
class CLAUDECORE_API UClcPauseMenuSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "ClcPauseMenu")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, Category = "ClcPauseMenu")
	bool OpenMenu();

	UFUNCTION(BlueprintCallable, Category = "ClcPauseMenu")
	void CloseMenu();

	bool IsMenuOpen() const { return bMenuOpen; }

	/** 关卡切换后 GameInstance 调用：重新绑定输入 */
	void RefreshInputBinding();

	// ---- Button callbacks ----
	void ResumeGame();
	void ManualSave();
	void LoadSave();
	void GoToMainMenu();
	void QuitGame();

private:
	bool CanOpenMenu() const;
	APlayerController* GetPlayerController() const;
	void EnsureInputBinding();
	void RemoveInputBinding();

	UPROPERTY(Transient)
	TObjectPtr<UClcPauseMenuWidget> MenuWidget;

	TSubclassOf<UClcPauseMenuWidget> MenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	uint32 InputBindingHandle = 0;
	bool bMenuOpen = false;
	bool bOwnsInputState = false;
	FTSTicker::FDelegateHandle TickerHandle;
};
