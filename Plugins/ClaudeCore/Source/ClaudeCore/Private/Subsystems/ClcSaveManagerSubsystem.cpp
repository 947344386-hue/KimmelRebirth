// Copyright ClaudeCore. All Rights Reserved.

#include "ClcGameInstance.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "ClcLog.h"
#include "Data/ClcSessionTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "GameFramework/PlayerController.h"
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

	if (WriteSaveFile(SlotName, Data))
	{
		CurrentSlot = SlotName;
		LastAutoSavedGold = Data.SavedGold;
		LastAutoSaveTime = FPlatformTime::Seconds();
		return true;
	}
	return false;
}

bool UClcSaveManagerSubsystem::LoadGame(const FString& SlotName)
{
	FClcSaveData Data;
	if (!ReadSaveFile(SlotName, Data)) return false;

	DistributeSaveData(Data);
	CurrentSlot = SlotName;
	LastAutoSavedGold = Data.SavedGold;
	LastAutoSaveTime = FPlatformTime::Seconds();

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
	if (!LP) return Data;

	if (UClcBackpackSubsystem* BP = LP->GetSubsystem<UClcBackpackSubsystem>())
	{
		Data.SavedStones = BP->GetStones();
		Data.SavedGold = BP->GetGoldValue();
		Data.SavedTotalEarned = BP->GetTotalEarned();
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
}

bool UClcSaveManagerSubsystem::WriteSaveFile(const FString& SlotName, const FClcSaveData& Data)
{
	USaveGame* Obj = UGameplayStatics::CreateSaveGameObject(USaveGame::StaticClass());
	if (!Obj) return false;
	FClcSaveData* Ptr = reinterpret_cast<FClcSaveData*>(Obj);
	*Ptr = Data;
	return UGameplayStatics::SaveGameToSlot(Obj, SlotName, 0);
}

bool UClcSaveManagerSubsystem::ReadSaveFile(const FString& SlotName, FClcSaveData& OutData) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	USaveGame* Obj = UGameplayStatics::LoadGameFromSlot(SlotName, 0);
	if (!Obj) return false;
	OutData = *reinterpret_cast<FClcSaveData*>(Obj);
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
