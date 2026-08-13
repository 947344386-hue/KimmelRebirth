// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Data/ClcSessionTypes.h"
#include "ClcGameInstance.generated.h"

class UClcSaveManagerSubsystem;
class UClcGameFlowManager;

/**
 * 自定义 GameInstance —— 打包游戏的总控。
 *
 * 职责：
 *  - 创建并持有所有 GameInstanceSubsystem（SaveManager、GameFlowManager 等）
 *  - 编排关卡转换（主菜单 ↔ 玩法）
 *  - 管理会话配置（起始金币、难度等）
 *  - 绑定 FCoreUObjectDelegates 处理加载画面和自动保存
 *
 * 在 DefaultEngine.ini 中设为 GameInstanceClass。
 */
UCLASS(Config=Game)
class CLAUDECORE_API UClcGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// ---- UGameInstance 重写 ----
	virtual void Init() override;
	virtual void Shutdown() override;

	// ---- 关卡生命周期回调（绑定到 FCoreUObjectDelegates） ----
	/** 关卡加载前——自动保存 */
	void HandlePreLoadMap(const FString& MapName);
	/** 关卡加载后——初始化会话状态 */
	void HandlePostLoadMap(UWorld* World);

	// ---- 会话管理 ----

	/** 创建新游戏——推入会话配置，加载玩法关卡。
	 *  @param Config 新游戏配置（起始金币、难度等）
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	void StartNewGame(const FClcSessionConfig& Config);

	/** 从存档继续——加载存档 → 分发到子系统 → 加载存档记录的关卡。
	 *  @param SlotName 存档槽位名
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	void LoadAndResumeGame(const FString& SlotName);

	/** 回到主菜单——自动保存当前进度 → 加载主菜单关卡。
	 *  @param bSaveFirst 是否先保存（默认 true）
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	void GoToMainMenu(bool bSaveFirst = true);

	/** 请求退出应用 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	void RequestQuit();

	// ---- 存取器 ----

	/** 当前会话配置 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	const FClcSessionConfig& GetSessionConfig() const { return CurrentSessionConfig; }

	/** 当前会话是否有效（是否已在游戏中） */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	bool IsInGameSession() const { return bInGameSession; }

	/** 是否正在关卡转换中 */
	UFUNCTION(BlueprintCallable, Category = "ClcGameInstance")
	bool IsTransitioning() const { return bIsTransitioning; }

	// ---- 运行时状态（public，SaveManager 等子系统可读写） ----

	/** 当前会话配置 */
	FClcSessionConfig CurrentSessionConfig;

	/** 设置会话配置（由 SaveManager 加载存档时调用） */
	void SetSessionConfig(const FClcSessionConfig& Cfg) { CurrentSessionConfig = Cfg; }

	/** 是否在游戏会话中（Map_JadePlayTest 内） */
	bool bInGameSession = false;

	/** 是否正在关卡转换中（防止双重保存、重复操作） */
	bool bIsTransitioning = false;

	/** 上次自动保存时的 Gold（用于增量触发） */
	int32 LastAutoSavedGold = 0;

	/** 上次自动保存时间 */
	double LastAutoSaveTime = 0.0;

	/** 读档时缓存的玩家坐标——关卡加载后 Pawn 就绪时应用 */
	FVector PendingPlayerLocation = FVector::ZeroVector;
	/** 读档时缓存的玩家朝向 */
	FRotator PendingPlayerRotation = FRotator::ZeroRotator;
	/** 是否有待应用的玩家坐标（区分新游戏 vs 读档） */
	bool bHasPendingPlayerTransform = false;
	/** ApplyPendingPlayerTransform 的重试计数（Pawn 未就绪时） */
	int32 PendingTransformAttempts = 0;

	/** 读档时缓存的摊位存档状态——等关卡加载后再分发到各摊位 */
	TMap<FName, struct FClcStallSaveState> CachedSavedStalls;
	bool bHasCachedSavedStalls = false;

	/** 自动保存间隔（秒，默认 300=5分钟） */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Save")
	float AutoSaveIntervalSeconds = 300.0f;

	/** 金币变动增量触发自动保存的阈值（默认 5000） */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Save")
	int32 AutoSaveGoldDeltaThreshold = 5000;

protected:
	/** 确保所有 GameInstanceSubsystem 已创建（懒初始化） */
	void EnsureSubsystemsReady();

	/** 遍历所有持有玩家数据的 Subsystem 并序列化到 SaveManager→SaveGame */
	void TriggerAutoSave();

	/** 读档后延迟应用缓存的玩家坐标（关卡加载完 Pawn 就绪时调用） */
	void ApplyPendingPlayerTransform();

	// ---- 配置（Project Settings → Plugins → ClaudeCore 或 Config/DefaultGame.ini） ----

	/** 主菜单关卡路径 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GameFlow")
	FString MainMenuLevelPath = TEXT("/Game/JadeBetting/Level/Map_MainMenu");

	/** 玩法关卡路径（新游戏默认目标） */
	UPROPERTY(Config, EditDefaultsOnly, Category = "GameFlow")
	FString DefaultGameLevelPath = TEXT("/Game/JadeBetting/Level/Map_JadePlayTest");

	/** FCoreUObjectDelegates 句柄（Shutdown 时移除） */
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
};
