// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ClcPauseMenuSubsystem.generated.h"

class UClcPauseMenuWidget;
class UClcSaveSlotListWidget;
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

	/** 打开 2 级槽位列表（Save=选槽写档 / Load=选槽读档） */
	void OpenSlotList(bool bLoadMode);

	/** 槽位列表结果回调 */
	UFUNCTION()
	void HandleSlotPicked(const FString& SlotName);

	/** 关闭 2 级槽位列表 */
	void CloseSlotList();

	UPROPERTY(Transient)
	TObjectPtr<UClcPauseMenuWidget> MenuWidget;

	/** 2 级槽位列表 Widget（手动存档/读取） */
	UPROPERTY(Transient)
	TObjectPtr<UClcSaveSlotListWidget> SlotListWidget;

	/** 槽位列表当前模式（Load=读档 / Save=写档） */
	bool bSlotListLoadMode = false;

	TSubclassOf<UClcPauseMenuWidget> MenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseMenuAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;
	uint32 InputBindingHandle = 0;
	bool bMenuOpen = false;
	bool bOwnsInputState = false;
	FTSTicker::FDelegateHandle TickerHandle;
};
