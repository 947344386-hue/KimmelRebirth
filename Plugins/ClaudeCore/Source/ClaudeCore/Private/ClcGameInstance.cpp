// Copyright ClaudeCore. All Rights Reserved.

#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "ClcDeveloperSettings.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

void UClcGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] Init —— GameInstance 启动"));

	// GameInstanceSubsystem（如 ClcStoneMarketSubsystem、ClcSaveManagerSubsystem 等）
	// 由引擎在 Init 期间自动创建，不需要手动 Spawn。

	bInGameSession = false;
	bIsTransitioning = false;

	// 绑定全局关卡转换委托——UE 5.6 的 UGameInstance::OnPreLoadMap/OnPostLoadMap 不是虚函数
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(
		this, &UClcGameInstance::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &UClcGameInstance::HandlePostLoadMap);
}

void UClcGameInstance::Shutdown()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] Shutdown —— 应用即将关闭"));

	// 如果在游戏会话中，最后自动保存一次
	if (bInGameSession && !bIsTransitioning)
	{
		TriggerAutoSave();
	}

	// 移除委托绑定
	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Shutdown();
}

void UClcGameInstance::HandlePreLoadMap(const FString& MapName)
{
	// 关卡转换前：如果在游戏中，先自动保存
	if (bInGameSession && !bIsTransitioning)
	{
		TriggerAutoSave();
	}

	bIsTransitioning = true;

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] HandlePreLoadMap: %s"), *MapName);
}

void UClcGameInstance::HandlePostLoadMap(UWorld* World)
{
	bIsTransitioning = false;

	if (!World)
	{
		return;
	}

	const FString MapName = World->GetMapName();
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] HandlePostLoadMap: %s"), *MapName);

	// 判断关卡类型
	const bool bIsMainMenu = MapName.Contains(TEXT("Map_MainMenu"));
	const bool bIsGameLevel = MapName.Contains(TEXT("Map_JadePlayTest"));

	if (bIsGameLevel)
	{
		bInGameSession = true;
		LastAutoSaveTime = FPlatformTime::Seconds();

		// 应用会话配置到 BackpackSubsystem（新游戏启动时的金币设置）
		if (CurrentSessionConfig.bIsNewGame)
		{
			if (const ULocalPlayer* LP = GetFirstGamePlayer())
			{
				if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
				{
					BP->SetSessionConfig(CurrentSessionConfig);
				}
			}
		}

		UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] 进入玩法关卡，会话已激活"));
	}
	else if (bIsMainMenu)
	{
		bInGameSession = false;
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] 进入主菜单关卡"));
	}
}

void UClcGameInstance::StartNewGame(const FClcSessionConfig& Config)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] StartNewGame —— StartingGold=%d, Difficulty=%d"),
		Config.StartingGold, static_cast<uint8>(Config.Difficulty));

	CurrentSessionConfig = Config;
	CurrentSessionConfig.bIsNewGame = true;

	// 用预设难度自动补全系数（Custom 模式保持用户值）
	if (Config.Difficulty != EClcDifficultyPreset::Custom)
	{
		CurrentSessionConfig.DifficultyMultiplier = ClcDifficultyPenaltyMultiplier(Config.Difficulty);
		CurrentSessionConfig.StartingGold = FMath::RoundToInt(
			static_cast<float>(Config.StartingGold) * ClcDifficultyGoldMultiplier(Config.Difficulty));
	}

	// 激活 SaveManager 槽位
	if (UClcSaveManagerSubsystem* SM = GetSubsystem<UClcSaveManagerSubsystem>())
	{
		SM->SetCurrentSlot(UClcSaveManagerSubsystem::AutoSaveSlotName);
	}

	// 加载玩法关卡——OnPostLoadMap 中会调用 BackpackSubsystem::SetSessionConfig
	const FString LevelPath = CurrentSessionConfig.GameLevelPath.IsEmpty()
		? DefaultGameLevelPath
		: CurrentSessionConfig.GameLevelPath;

	bIsTransitioning = true;
	UGameplayStatics::OpenLevel(this, FName(*LevelPath));
}

void UClcGameInstance::LoadAndResumeGame(const FString& SlotName)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] LoadAndResumeGame —— Slot=%s"), *SlotName);

	// 委托 SaveManager 加载存档 → 分发到所有子系统
	if (UClcSaveManagerSubsystem* SM = GetSubsystem<UClcSaveManagerSubsystem>())
	{
		if (!SM->LoadGame(SlotName))
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcGameInstance] LoadAndResumeGame —— 加载存档失败！"));
			return;
		}
	}

	CurrentSessionConfig.SaveSlotName = SlotName;
	CurrentSessionConfig.bIsNewGame = false;

	// 加载存档中记录的关卡（默认玩法关卡）
	bIsTransitioning = true;
	UGameplayStatics::OpenLevel(this, FName(*DefaultGameLevelPath));
}

void UClcGameInstance::GoToMainMenu(bool bSaveFirst)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] GoToMainMenu —— bSaveFirst=%d"), bSaveFirst);

	if (bSaveFirst && bInGameSession)
	{
		TriggerAutoSave();
	}

	bInGameSession = false;
	bIsTransitioning = true;
	UGameplayStatics::OpenLevel(this, FName(*MainMenuLevelPath));
}

void UClcGameInstance::RequestQuit()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] RequestQuit"));

	if (bInGameSession)
	{
		TriggerAutoSave();
	}

	// 获取当前世界玩家的 Controller 并调用 ConsoleCommand
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("quit"));
			return;
		}
	}

	// 兜底：直接请求引擎退出
	FPlatformMisc::RequestExit(false);
}

void UClcGameInstance::EnsureSubsystemsReady()
{
	// GameInstanceSubsystem 在 Init 后自动就绪，
	// 这里做一次显式获取确保空引用已填充。
	// 实际序列化/反序列化逻辑在后续 Phase 实现。
}

void UClcGameInstance::TriggerAutoSave()
{
	if (UClcSaveManagerSubsystem* SM = GetSubsystem<UClcSaveManagerSubsystem>())
	{
		SM->TriggerAutoSave();
	}
	else
	{
		UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcGameInstance] TriggerAutoSave —— SaveManager 不可用"));
	}
}
