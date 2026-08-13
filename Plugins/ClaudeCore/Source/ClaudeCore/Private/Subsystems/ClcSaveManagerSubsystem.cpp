// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "ClcGameInstance.h"
#include "Actors/ClcStoneStall.h"
#include "ClcLog.h"
#include "Data/ClcSessionTypes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

const FString UClcSaveManagerSubsystem::AutoSaveSlotName = TEXT("AutoSave");
const FString UClcSaveManagerSubsystem::ManualSlotPrefix = TEXT("ManualSlot_");

FString UClcSaveManagerSubsystem::MakeManualSlotName(int32 Index)
{
	return FString::Printf(TEXT("%s%d"), *ManualSlotPrefix, Index);
}

void UClcSaveManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] Initialize"));
	UGameInstance* GI = GetGameInstance();
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
	{
		AutoSaveIntervalSeconds = ClcGI->AutoSaveIntervalSeconds;
		AutoSaveGoldDeltaThreshold = ClcGI->AutoSaveGoldDeltaThreshold;
	}
	LastAutoSaveTime = FPlatformTime::Seconds();
	RestartAutoSaveTimer();
}

void UClcSaveManagerSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	Super::Deinitialize();
}

TArray<FClcSaveMetaData> UClcSaveManagerSubsystem::GetSaveSlots() const
{
	TArray<FClcSaveMetaData> Slots;
	FClcSaveMetaData Meta;
	if (UGameplayStatics::DoesSaveGameExist(AutoSaveSlotName, 0))
	{
		if (ReadMetaData(AutoSaveSlotName, Meta)) Slots.Add(Meta);
	}
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		const FString SlotName = MakeManualSlotName(i);
		if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		{
			if (ReadMetaData(SlotName, Meta)) Slots.Add(Meta);
		}
	}
	return Slots;
}

bool UClcSaveManagerSubsystem::HasAnySave() const
{
	if (UGameplayStatics::DoesSaveGameExist(AutoSaveSlotName, 0))
	{
		FClcSaveMetaData Meta;
		if (ReadMetaData(AutoSaveSlotName, Meta)) return true;
	}
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (UGameplayStatics::DoesSaveGameExist(MakeManualSlotName(i), 0))
		{
			FClcSaveMetaData Meta;
			if (ReadMetaData(MakeManualSlotName(i), Meta)) return true;
		}
	}
	return false;
}

// ---- 保存/加载 ----

bool UClcSaveManagerSubsystem::SaveGame(const FString& SlotName)
{
	FClcSaveData Data = CollectSaveData();
	Data.SaveTimestamp = FDateTime::Now();
	Data.SaveVersion = ClcCurrentSaveVersion();
	if (UWorld* World = GetWorld())
		Data.LevelName = World->GetMapName();

	// 只在所有数据源都为空（从未玩过的空状态）时拒绝写。
	// 之前的条件只看金币/石头/摊位，会误拒"花光+空背包但工具/任务有进度"的合法档。
	if (Data.SavedGold == 0 && Data.SavedStones.Num() == 0
		&& Data.SavedTotalEarned == 0 && Data.SavedStalls.Num() == 0
		&& Data.SavedDurability.Num() == 0 && Data.SavedUpgrades.Num() == 0
		&& Data.SavedQuestStates.Num() == 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] SaveGame '%s': 全空状态，拒绝写空存档"), *SlotName);
		return false;
	}

	// 累计游戏时长（仅在写盘成功后才累加，避免失败/拦截时重复累计污染下次保存）
	double Now = FPlatformTime::Seconds();
	float DeltaSec = static_cast<float>(Now - LastAutoSaveTime);
	if (DeltaSec > 0.0f && DeltaSec < 3600.0f) // 防异常跳变
	{
		AccumulatedPlayTimeSeconds += DeltaSec;
	}
	Data.PlayTimeHours = AccumulatedPlayTimeSeconds / 3600.0f;

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] SaveGame '%s': Gold=%d, Stones=%d, PlayTime=%.2fh, Ver=%d"),
		*SlotName, Data.SavedGold, Data.SavedStones.Num(), Data.PlayTimeHours, Data.SaveVersion);

	if (WriteSaveFile(SlotName, Data))
	{
		CurrentSlot = SlotName;
		LastAutoSavedGold = Data.SavedGold;
		LastAutoSaveTime = Now;
		return true;
	}
	NotifySaveFailedToast(SlotName, TEXT("写盘失败"));
	return false;
}

bool UClcSaveManagerSubsystem::LoadGame(const FString& SlotName)
{
	FClcSaveData Data;
	if (!ReadSaveFile(SlotName, Data))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] LoadGame '%s': 读取失败或存档损坏"), *SlotName);
		NotifySaveFailedToast(SlotName, TEXT("存档损坏或读取失败"));
		return false;
	}

	// 版本兼容性检查：旧字符串档反序列化后 SaveVersion 取默认值 0（字段类型变更），
	// 视为版本 0 拒绝加载。当前版本 3，无历史迁移步骤。
	if (Data.SaveVersion < ClcCurrentSaveVersion())
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] LoadGame '%s': 存档版本 %d < 当前 %d，拒绝加载（旧版或字符串档）"),
			*SlotName, Data.SaveVersion, ClcCurrentSaveVersion());
		NotifySaveFailedToast(SlotName, TEXT("存档版本不兼容"));
		return false;
	}

	// 未来迁移链入口（当前无历史步骤）：
	// for (int32 V = Data.SaveVersion; V < ClcCurrentSaveVersion(); ++V)
	// {
	//     Migrate(V, V + 1, Data);
	// }

	if (!DistributeSaveData(Data))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] LoadGame '%s': 数据分发失败（子系统未就绪）"), *SlotName);
		NotifySaveFailedToast(SlotName, TEXT("数据恢复失败"));
		return false;
	}

	CurrentSlot = SlotName;
	LastAutoSavedGold = Data.SavedGold;
	LastAutoSaveTime = FPlatformTime::Seconds();
	AccumulatedPlayTimeSeconds = Data.PlayTimeHours * 3600.0f;

	UGameInstance* GI = GetGameInstance();
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
	{
		ClcGI->SetSessionConfig(Data.SessionConfig);
	}
	RestartAutoSaveTimer();
	return true;
}

bool UClcSaveManagerSubsystem::DeleteSave(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	if (!UGameplayStatics::DeleteGameInSlot(SlotName, 0)) return false;
	if (CurrentSlot == SlotName) CurrentSlot.Empty();
	return true;
}

// ---- 自动保存 ----

void UClcSaveManagerSubsystem::TriggerAutoSave()
{
	if (!bAutoSaveEnabled) return;
	// 自动保存永远写 AutoSave 槽，不随手动存档/读档改变目标
	SaveGame(AutoSaveSlotName);
}

void UClcSaveManagerSubsystem::SetAutoSaveEnabled(bool bEnabled)
{
	bAutoSaveEnabled = bEnabled;
	if (bEnabled) RestartAutoSaveTimer();
	else if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
}

void UClcSaveManagerSubsystem::NotifyGoldChanged(int32 NewGold)
{
	if (!bAutoSaveEnabled) return;
	if (FMath::Abs(NewGold - LastAutoSavedGold) >= AutoSaveGoldDeltaThreshold)
		SaveGame(AutoSaveSlotName);
}

void UClcSaveManagerSubsystem::NotifyTransactionCompleted()
{
	SaveGame(AutoSaveSlotName);
}

// ---- 内部 ----

FClcSaveData UClcSaveManagerSubsystem::CollectSaveData() const
{
	FClcSaveData Data;
	Data.SaveVersion = ClcCurrentSaveVersion();

	UGameInstance* GI = GetGameInstance();
	if (!GI) return Data;

	const ULocalPlayer* LP = GI->GetFirstGamePlayer();
	if (!LP)
	{
		// Shutdown 阶段 LocalPlayer 已销毁。不要用 LastAutoSavedGold 兜底写半截存档——
		// 那会覆盖之前定时器写的完整存档（石头/工具全丢）。返回空 Data，由 SaveGame 的空状态拦截放弃写盘。
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] CollectSaveData: LP null（Shutdown？），放弃本次收集，保留已有存档"));
		return Data;
	}

	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
	{
		Data.SavedStones = BP->GetStones();
		Data.SavedGold = BP->GetGoldValue();
		Data.SavedTotalEarned = BP->GetTotalEarned();
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] CollectSaveData: Gold=%d, Stones=%d"), Data.SavedGold, Data.SavedStones.Num());
	}

	// 摊位槽位：按 GetPathName（Id）分组收集，读档时每个摊位只恢复自己那份
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AClcStoneStall> It(World); It; ++It)
		{
			AClcStoneStall* Stall = *It;
			if (!IsValid(Stall)) continue;
			const FName Id = Stall->GetStallId();
			FClcStallSaveState& State = Data.SavedStalls.FindOrAdd(Id);
			State.StallId = Id;
			Stall->CollectSlots(State.Slots);
		}
		int32 TotalSlots = 0;
		for (const auto& Pair : Data.SavedStalls) TotalSlots += Pair.Value.Slots.Num();
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] CollectSaveData: %d stalls, %d slots"),
			Data.SavedStalls.Num(), TotalSlots);
	}

	if (UClcToolDurabilitySubsystem* TD = LP->GetSubsystem<UClcToolDurabilitySubsystem>())
	{
		const UEnum* ToolEnum = StaticEnum<EClcRepairableTool>();
		for (int32 i = 0; i < ToolEnum->NumEnums() - 1; ++i)
		{
			EClcRepairableTool Tool = static_cast<EClcRepairableTool>(ToolEnum->GetValueByIndex(i));
			if (Tool == EClcRepairableTool::None) continue;
			float MaxDur = TD->GetMaxDurability(Tool);
			if (MaxDur > 0.0f)
			{
				Data.SavedDurability.Add(static_cast<int32>(Tool), TD->GetDurability(Tool));
				Data.SavedMaxDurability.Add(static_cast<int32>(Tool), MaxDur);
			}
		}
		const UEnum* UpgEnum = StaticEnum<EClcToolUpgrade>();
		for (int32 i = 0; i < UpgEnum->NumEnums() - 1; ++i)
		{
			EClcToolUpgrade Upg = static_cast<EClcToolUpgrade>(UpgEnum->GetValueByIndex(i));
			if (TD->OwnsUpgrade(Upg)) Data.SavedUpgrades.Add(static_cast<int32>(Upg));
		}
	}
	if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
	{
		QS->SerializeForSave(Data.SavedQuestStates);
	}
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
		Data.SessionConfig = ClcGI->GetSessionConfig();

	// 玩家位置/朝向
	if (APlayerController* PC = LP->GetPlayerController(GetWorld()))
	{
		if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			Data.SavedPlayerLocation = Pawn->GetActorLocation();
			Data.SavedPlayerRotation = Pawn->GetActorRotation();
			Data.bHasPlayerTransform = true;
		}
	}

	return Data;
}

bool UClcSaveManagerSubsystem::DistributeSaveData(const FClcSaveData& Data)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return false;

	ULocalPlayer* LP = GI->GetFirstGamePlayer();
	if (!LP) return false;

	bool bAllOk = true;

	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
		BP->RestoreFromSaveData(Data);
	else bAllOk = false;
	if (UClcToolDurabilitySubsystem* TD = LP->GetSubsystem<UClcToolDurabilitySubsystem>())
		TD->RestoreFromSaveData(Data);
	else bAllOk = false;
	if (UClcQuestSubsystem* QS = LP->GetSubsystem<UClcQuestSubsystem>())
		QS->RestoreFromSaveData(Data, /*bIsNewGame=*/false);
	else bAllOk = false;

	// 摊位槽位缓存到 GameInstance——等关卡加载完毕再将各摊位自己的 Slots 分发
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
	{
		ClcGI->CachedSavedStalls = Data.SavedStalls;
		ClcGI->bHasCachedSavedStalls = true;
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] DistributeSaveData: Cached %d stall states for post-load restore"),
			Data.SavedStalls.Num());
	}
	else bAllOk = false;

	// 玩家坐标缓存到 GameInstance，等关卡加载后 Pawn 就绪再应用
	// （此处可能还在旧关卡，Pawn 不可用）
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
	{
		if (Data.bHasPlayerTransform)
		{
			ClcGI->PendingPlayerLocation = Data.SavedPlayerLocation;
			ClcGI->PendingPlayerRotation = Data.SavedPlayerRotation;
			ClcGI->bHasPendingPlayerTransform = true;
		}
	}

	return bAllOk;
}

bool UClcSaveManagerSubsystem::WriteSaveFile(const FString& SlotName, const FClcSaveData& Data)
{
	UClcPlayerSaveGame* SaveObj = Cast<UClcPlayerSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UClcPlayerSaveGame::StaticClass()));
	if (!SaveObj) return false;
	SaveObj->SaveData = Data;

	// 原子写：先序列化到内存，写 .sav.tmp，回读 size 校验，再 rename 替换原文件。
	// 引擎 SaveGameToSlot 内部直接覆盖目标文件无原子性，项目侧自行兜底。
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(SaveObj, Bytes))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] WriteSaveFile '%s': SaveGameToMemory 失败"), *SlotName);
		return false;
	}

	IFileManager& FM = IFileManager::Get();
	const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	const FString FinalPath = SaveDir / (SlotName + TEXT(".sav"));
	const FString TmpPath = SaveDir / (SlotName + TEXT(".sav.tmp"));

	// 写临时文件
	if (!FFileHelper::SaveArrayToFile(Bytes, *TmpPath))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] WriteSaveFile '%s': 写临时文件失败"), *SlotName);
		return false;
	}

	// 回读 size 校验——防写盘中途断电/磁盘满产出的半截文件
	const int64 TmpSize = FM.FileSize(*TmpPath);
	if (TmpSize != static_cast<int64>(Bytes.Num()))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] WriteSaveFile '%s': 临时文件 size 校验失败 (%lld != %d)，保留原档"),
			*SlotName, TmpSize, Bytes.Num());
		FM.Delete(*TmpPath, false, false, true);
		return false;
	}

	// 同盘 rename 替换原文件（比直接覆盖写更原子）
	if (!FM.Move(*FinalPath, *TmpPath, true, true, true, true))
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcSave] WriteSaveFile '%s': Move 替换失败，保留原档"), *SlotName);
		FM.Delete(*TmpPath, false, false, true);
		return false;
	}

	return true;
}

bool UClcSaveManagerSubsystem::ReadSaveFile(const FString& SlotName, FClcSaveData& OutData) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	UClcPlayerSaveGame* SaveObj = Cast<UClcPlayerSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;
	OutData = SaveObj->SaveData;

	// 内容校验——半损坏但仍可反序列化的档不直接覆盖运行态
	if (!ValidateSaveData(OutData))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ReadSaveFile '%s': 内容校验失败，视为坏档"), *SlotName);
		return false;
	}
	return true;
}

bool UClcSaveManagerSubsystem::ValidateSaveData(const FClcSaveData& Data) const
{
	// 背包石头数上限
	if (Data.SavedStones.Num() > 200)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: SavedStones.Num()=%d 超 200"), Data.SavedStones.Num());
		return false;
	}
	// 金币/累计收益
	if (Data.SavedGold < 0 || Data.SavedTotalEarned < 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: Gold=%d 或 TotalEarned=%d 为负"), Data.SavedGold, Data.SavedTotalEarned);
		return false;
	}
	// 摊位每个 Slots 数上限
	for (const auto& Pair : Data.SavedStalls)
	{
		if (Pair.Value.Slots.Num() > 200)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: 摊位 %s Slots=%d 超 200"), *Pair.Value.StallId.ToString(), Pair.Value.Slots.Num());
			return false;
		}
	}
	// 游戏时长合理范围
	if (Data.PlayTimeHours < 0.0f || Data.PlayTimeHours >= 100000.0f)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: PlayTimeHours=%.2f 越界"), Data.PlayTimeHours);
		return false;
	}
	// 工具耐久值范围
	for (const auto& Pair : Data.SavedDurability)
	{
		if (Pair.Value < 0.0f || Pair.Value > 100000.0f)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: SavedDurability[key=%d]=%.2f 越界"), Pair.Key, Pair.Value);
			return false;
		}
	}
	for (const auto& Pair : Data.SavedMaxDurability)
	{
		if (Pair.Value < 0.0f || Pair.Value > 100000.0f)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] ValidateSaveData: SavedMaxDurability[key=%d]=%.2f 越界"), Pair.Key, Pair.Value);
			return false;
		}
	}
	return true;
}

bool UClcSaveManagerSubsystem::ReadMetaData(const FString& SlotName, FClcSaveMetaData& OutMeta) const
{
	OutMeta = FClcSaveMetaData();
	OutMeta.SlotName = SlotName;
	FClcSaveData Data;
	if (!ReadSaveFile(SlotName, Data)) return false;
	OutMeta.SaveTimestamp = Data.SaveTimestamp;
	OutMeta.Gold = Data.SavedGold;
	OutMeta.StoneCount = Data.SavedStones.Num();
	OutMeta.PlayTimeHours = Data.PlayTimeHours;
	OutMeta.LevelName = Data.LevelName;
	OutMeta.SaveVersion = Data.SaveVersion;
	return true;
}

void UClcSaveManagerSubsystem::RestartAutoSaveTimer()
{
	UWorld* World = GetWorld();
	if (!World || !bAutoSaveEnabled) return;
	World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
	World->GetTimerManager().SetTimer(AutoSaveTimerHandle,
		FTimerDelegate::CreateUObject(this, &UClcSaveManagerSubsystem::TriggerAutoSave),
		AutoSaveIntervalSeconds, true);
}

void UClcSaveManagerSubsystem::NotifySaveFailedToast(const FString& SlotName, const FString& Reason) const
{
	if (const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr)
	{
		if (UClcLogToastSubsystem* Toast = LP->GetSubsystem<UClcLogToastSubsystem>())
		{
			Toast->AddLog(FString::Printf(TEXT("存档失败：%s（%s）"), *SlotName, *Reason),
				3.0f, FLinearColor(0.9f, 0.2f, 0.2f));
		}
	}
}
