// Copyright ClaudeCore. All Rights Reserved.

#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "ClcDeveloperSettings.h"
#include "UI/ClcLoadingScreenWidget.h"
#include "MoviePlayer.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcPauseMenuSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "Actors/ClcStoneStall.h"
#include "Data/ClcSessionTypes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

	// 注入 MoviePlayer loading screen —— 引擎级加载画面（背景图轮播）。
	// 只在打包游戏生效（!GIsEditor），PIE 里 MoviePlayer 不跑，此调用无副作用。
	SetupLoadingScreen();
}

void UClcGameInstance::Shutdown()
{
	// 抢在所有 LocalPlayerSubsystem 销毁之前保存。
	// 收集数据 → 直接写文件，不依赖任何 LocalPlayer。
	if (bInGameSession)
	{
		if (UClcSaveManagerSubsystem* SM = GetSubsystem<UClcSaveManagerSubsystem>())
		{
			SM->TriggerAutoSave();
		}
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

		// 新游戏：把起始金币/难度写到 Backpack。读档：LoadGame 已经恢复过数据，这里跳过。
		if (CurrentSessionConfig.bIsNewGame)
		{
			if (const ULocalPlayer* LP = GetFirstGamePlayer())
			{
				if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
				{
					BP->SetSessionConfig(CurrentSessionConfig);
				}
				// 新游戏：接取所有链首任务
				if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
				{
					QS->AcceptAllQuests();
				}
			}
		}

		// 进入玩法关卡后重启自动保存定时器（Initialize 阶段 World 为空无法启动）
		if (UClcSaveManagerSubsystem* SM = GetSubsystem<UClcSaveManagerSubsystem>())
		{
			SM->SetAutoSaveEnabled(true);
		}

		// 关卡切换后重建常驻 HUD + 重绑 PauseMenu 输入 + 重建任务追踪面板
		if (const ULocalPlayer* LP = GetFirstGamePlayer())
		{
			if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
			{
				BP->RebuildHud();
			}
			if (UClcPauseMenuSubsystem* PM = LP->GetSubsystem<UClcPauseMenuSubsystem>())
			{
				PM->RefreshInputBinding();
			}
			if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
			{
				QS->RebuildTracker();
			}
		}

		// 读档恢复：摊位槽位数据——每个摊位只恢复自己那份
		if (bHasCachedSavedStalls)
		{
			if (UWorld* W = GetWorld())
			{
				W->GetTimerManager().SetTimerForNextTick([this]()
				{
					for (TActorIterator<AClcStoneStall> It(GetWorld()); It; ++It)
					{
						AClcStoneStall* Stall = *It;
						if (!IsValid(Stall)) continue;
						const FClcStallSaveState* State = CachedSavedStalls.Find(Stall->GetStallId());
						if (State)
						{
							Stall->RestoreFromSlots(State->Slots);
						}
					}
					CachedSavedStalls.Empty();
					bHasCachedSavedStalls = false;
					UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] Stall stones restored from save cache"));
				});
			}
		}

		// 读档恢复：如果有缓存的玩家坐标，延迟一帧应用（等 Pawn Spawn）
		if (bHasPendingPlayerTransform)
		{
			if (UWorld* W = GetWorld())
			{
				W->GetTimerManager().SetTimerForNextTick([this]()
				{
					ApplyPendingPlayerTransform();
				});
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

	// 先标记 transitioning，避免 HandlePreLoadMap 再存一次（双重保存去重）
	bIsTransitioning = true;
	if (bSaveFirst && bInGameSession)
	{
		TriggerAutoSave();
	}

	bInGameSession = false;
	UGameplayStatics::OpenLevel(this, FName(*MainMenuLevelPath));
}

void UClcGameInstance::RequestQuit()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] RequestQuit"));

	// 先标记 transitioning，避免 HandlePreLoadMap 重复保存
	bIsTransitioning = true;
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

void UClcGameInstance::SetupLoadingScreen()
{
	IGameMoviePlayer* MP = GetMoviePlayer();
	if (!MP)
	{
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] SetupLoadingScreen —— MoviePlayer 不可用（PIE/编辑器正常）"));
		return;
	}

	FLoadingScreenAttributes Attrs;
	Attrs.WidgetLoadingScreen = SNew(SClcLoadingScreenWidget);
	Attrs.bAutoCompleteWhenLoadingCompletes = true;  // 加载完成自动收尾
	Attrs.bAllowEngineTick = true;                    // 让 WaitForMovieToFinish 跑 SlateApp.Tick 驱动轮播
	Attrs.bMoviesAreSkippable = false;
	Attrs.bWaitForManualStop = false;
	Attrs.MinimumLoadingScreenDisplayTime = -1.0f;   // 不强制最短显示时长

	MP->SetupLoadingScreen(Attrs);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] Loading screen 注入完成"));
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

void UClcGameInstance::ApplyPendingPlayerTransform()
{
	if (!bHasPendingPlayerTransform) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		// Pawn 还没就绪，下一 tick 重试（最多 60 次 = 1 秒）
		if (++PendingTransformAttempts < 60)
		{
			World->GetTimerManager().SetTimerForNextTick([this]() { ApplyPendingPlayerTransform(); });
		}
		else
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcGameInstance] ApplyPendingPlayerTransform —— Pawn 1 秒内未就绪，放弃"));
			bHasPendingPlayerTransform = false;
			PendingTransformAttempts = 0;
		}
		return;
	}

	// 用 TeleportTo（含碰撞检查，比直接 SetActorLocation 更安全）
	Pawn->TeleportTo(PendingPlayerLocation, PendingPlayerRotation, false, false);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcGameInstance] ApplyPendingPlayerTransform —— Loc=%s, Rot=%s"),
		*PendingPlayerLocation.ToString(), *PendingPlayerRotation.ToString());

	bHasPendingPlayerTransform = false;
	PendingTransformAttempts = 0;
}
