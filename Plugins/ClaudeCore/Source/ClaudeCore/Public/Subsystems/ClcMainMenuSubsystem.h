// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Data/ClcSessionTypes.h"
#include "UI/ClcSaveSlotListWidget.h"
#include "ClcMainMenuSubsystem.generated.h"

class UClcMainMenuWidget;
class UClcSaveSlotListWidget;

/**
 * 主菜单子系统 —— 管理主菜单 Widget 生命周期和会话配置。
 *
 * 生命周期：ULocalPlayerSubsystem，本地玩家加入时创建。
 * 模式：参照 UClcTeleportSubsystem——子系统拥有 Widget，创建/销毁/显隐。
 *
 * ZOrder：0（全屏覆盖，最低层——菜单是游戏最底层 UI）。
 * 输入模式：UIOnly（菜单打开时完全阻断游戏输入）。
 */
UCLASS()
class CLAUDECORE_API UClcMainMenuSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- 主菜单显隐 ----

	/** 创建并显示主菜单 Widget（首次调用）/仅设置可见（后续调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void ShowMainMenu();

	/** 隐藏并移除主菜单 Widget */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void HideMainMenu();

	/** 主菜单是否可见 */
	UFUNCTION(BlueprintPure, Category = "ClcMainMenu")
	bool IsMainMenuVisible() const { return bMenuVisible; }

	// ---- 操作 ----

	/** 开始新游戏——从 UI 读取配置，调用 GameInstance->StartNewGame */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void StartNewGame(const FClcSessionConfig& Config);

	/** 继续游戏——打开 2 级槽位列表（Load 模式），玩家选档后加载 */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void OpenContinueSlotList();

	/** 删除存档——打开 2 级槽位列表（Delete 模式），玩家选档确认后删除 */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void OpenDeleteSlotList();

	/** 加载指定存档并恢复（槽位列表选中后内部调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void ContinueGame(const FString& SlotName);

	/** 请求退出应用 */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	void QuitGame();

	/** 获取所有存档槽位元数据（菜单列表用） */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	TArray<FClcSaveMetaData> GetSaveSlots() const;

	/** 删除指定存档 */
	UFUNCTION(BlueprintCallable, Category = "ClcMainMenu")
	bool DeleteSave(const FString& SlotName);

	// ---- 配置 ----

	/** 默认会话配置（UI 初始化时的默认值） */
	UFUNCTION(BlueprintPure, Category = "ClcMainMenu")
	FClcSessionConfig GetDefaultSessionConfig() const { return DefaultSessionConfig; }

private:
	APlayerController* GetPlayerController() const;

	/** 打开 2 级槽位列表（指定模式） */
	void OpenSlotList(EClcSaveSlotListMode Mode);

	/** 槽位列表结果回调 */
	UFUNCTION()
	void HandleSlotPicked(const FString& SlotName);

	/** 关闭 2 级槽位列表 */
	void CloseSlotList();

	/** 主菜单 Widget 实例 */
	UPROPERTY(Transient)
	TObjectPtr<UClcMainMenuWidget> MenuWidget;

	/** 2 级槽位列表 Widget（继续游戏选档） */
	UPROPERTY(Transient)
	TObjectPtr<UClcSaveSlotListWidget> SlotListWidget;

	/** Widget 类——BP 子类可通过 Project Settings 或 Config 覆盖 */
	UPROPERTY(EditDefaultsOnly, Category = "ClcMainMenu")
	TSubclassOf<UClcMainMenuWidget> MenuWidgetClass;

	/** UI 是否可见 */
	bool bMenuVisible = false;

	/** 默认会话配置 */
	FClcSessionConfig DefaultSessionConfig;
};
