// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "Data/ClcSessionTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"

const FString UClcSaveManagerSubsystem::AutoSaveSlotName = TEXT("AutoSave");

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
	if (UGameplayStatics::DoesSaveGameExist(AutoSaveSlotName, 0))
		Slots.Add(ReadMetaData(AutoSaveSlotName));
	return Slots;
}

bool UClcSaveManagerSubsystem::HasAnySave() const
{
	return UGameplayStatics::DoesSaveGameExist(AutoSaveSlotName, 0);
}

// ---- 保存/加载 ----

bool UClcSaveManagerSubsystem::SaveGame(const FString& SlotName)
{
	FClcSaveData Data = CollectSaveData();
	Data.SaveTimestamp = FDateTime::Now();
	if (UWorld* World = GetWorld())
		Data.LevelName = World->GetMapName();

	// 累计游戏时长：上次保存到现在的间隔 + 已累计
	double Now = FPlatformTime::Seconds();
	float DeltaSec = static_cast<float>(Now - LastAutoSaveTime);
	if (DeltaSec > 0.0f && DeltaSec < 3600.0f) // 防异常跳变
	{
		AccumulatedPlayTimeSeconds += DeltaSec;
	}
	Data.PlayTimeHours = AccumulatedPlayTimeSeconds / 3600.0f;

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcSave] SaveGame '%s': Gold=%d, Stones=%d, PlayTime=%.2fh"),
		*SlotName, Data.SavedGold, Data.SavedStones.Num(), Data.PlayTimeHours);

	// 只在 Gold=0 && Stones=0 && TotalEarned=0（从未玩过的空状态）时拒绝写，
	// 避免覆盖已有完整存档。合法的"花光+空背包"状态允许保存。
	if (Data.SavedGold == 0 && Data.SavedStones.Num() == 0 && Data.SavedTotalEarned == 0)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] SaveGame '%s': 全空状态，拒绝写空存档"), *SlotName);
		return false;
	}
	if (WriteSaveFile(SlotName, Data))
	{
		CurrentSlot = SlotName;
		LastAutoSavedGold = Data.SavedGold;
		LastAutoSaveTime = Now;
		return true;
	}
	return false;
}

bool UClcSaveManagerSubsystem::LoadGame(const FString& SlotName)
{
	FClcSaveData Data;
	if (!ReadSaveFile(SlotName, Data)) return false;

	// 版本兼容性检查：版本不匹配则拒绝加载，避免数据结构错乱
	if (!Data.SaveVersion.IsEmpty() && Data.SaveVersion != TEXT("1.0"))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcSave] LoadGame '%s': 存档版本 %s 不兼容当前版本 1.0，拒绝加载"), *SlotName, *Data.SaveVersion);
		return false;
	}

	DistributeSaveData(Data);
	CurrentSlot = SlotName;
	LastAutoSavedGold = Data.SavedGold;
	LastAutoSaveTime = FPlatformTime::Seconds();
	AccumulatedPlayTimeSeconds = Data.PlayTimeHours * 3600.0f; // 从存档恢复累计时长

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
	if (CurrentSlot.IsEmpty()) CurrentSlot = AutoSaveSlotName;
	SaveGame(CurrentSlot);
}

void UClcSaveManagerSubsystem::SetAutoSaveEnabled(bool bEnabled)
{
	bAutoSaveEnabled = bEnabled;
	if (bEnabled) RestartAutoSaveTimer();
	else if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
}

void UClcSaveManagerSubsystem::NotifyGoldChanged(int32 NewGold)
{
	if (!bAutoSaveEnabled || CurrentSlot.IsEmpty()) return;
	if (FMath::Abs(NewGold - LastAutoSavedGold) >= AutoSaveGoldDeltaThreshold)
		SaveGame(CurrentSlot);
}

void UClcSaveManagerSubsystem::NotifyTransactionCompleted()
{
	if (CurrentSlot.IsEmpty()) CurrentSlot = AutoSaveSlotName;
	SaveGame(CurrentSlot);
}

// ---- 内部 ----

FClcSaveData UClcSaveManagerSubsystem::CollectSaveData() const
{
	FClcSaveData Data;
	Data.SaveVersion = TEXT("1.0");

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

void UClcSaveManagerSubsystem::DistributeSaveData(const FClcSaveData& Data)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;

	ULocalPlayer* LP = GI->GetFirstGamePlayer();
	if (!LP) return;

	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
		BP->RestoreFromSaveData(Data);
	if (UClcToolDurabilitySubsystem* TD = LP->GetSubsystem<UClcToolDurabilitySubsystem>())
		TD->RestoreFromSaveData(Data);

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
}

bool UClcSaveManagerSubsystem::WriteSaveFile(const FString& SlotName, const FClcSaveData& Data)
{
	UClcPlayerSaveGame* SaveObj = Cast<UClcPlayerSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UClcPlayerSaveGame::StaticClass()));
	if (!SaveObj) return false;
	SaveObj->SaveData = Data;
	return UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
}

bool UClcSaveManagerSubsystem::ReadSaveFile(const FString& SlotName, FClcSaveData& OutData) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	UClcPlayerSaveGame* SaveObj = Cast<UClcPlayerSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj) return false;
	OutData = SaveObj->SaveData;
	return true;
}

FClcSaveMetaData UClcSaveManagerSubsystem::ReadMetaData(const FString& SlotName) const
{
	FClcSaveMetaData Meta;
	Meta.SlotName = SlotName;
	FClcSaveData Data;
	if (ReadSaveFile(SlotName, Data))
	{
		Meta.SaveTimestamp = Data.SaveTimestamp;
		Meta.Gold = Data.SavedGold;
		Meta.StoneCount = Data.SavedStones.Num();
		Meta.PlayTimeHours = Data.PlayTimeHours;
		Meta.LevelName = Data.LevelName;
		Meta.SaveVersion = Data.SaveVersion;
	}
	return Meta;
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
