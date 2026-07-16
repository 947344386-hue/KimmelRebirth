// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcLogToastSubsystem.h"
#include "ClcLog.h"
#include "UI/ClcLogToastListWidget.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"

void UClcLogToastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 启动倒计时 Timer（0.1s 间隔）
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TickHandle, this, &UClcLogToastSubsystem::OnTick, 0.1f, true);
	}

	// ListWidget 不在这创建——Initialize 时 PlayerController 可能还没 spawn。
	// 改为 AddLog 时懒加载（那时 PC 肯定有了）。
}

void UClcLogToastSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TickHandle);
	}

	if (ListWidget)
	{
		ListWidget->RemoveFromParent();
		ListWidget = nullptr;
	}

	Entries.Empty();
	NextLogId = 0;

	Super::Deinitialize();
}

int32 UClcLogToastSubsystem::AddLog(const FString& Message, float Duration, FLinearColor Color)
{
	// 懒加载 ListWidget（Initialize 时 PC 可能没准备好）
	if (!ListWidget)
	{
		CreateListWidget();
	}

	// 去重：已有相同 Message 的 log → 只重置存活时间，不新增控件
	for (FLogEntry& E : Entries)
	{
		if (E.Message == Message)
		{
			E.RemainingTime = Duration;
			return E.Id;
		}
	}

	const int32 LogId = NextLogId++;

	FLogEntry Entry;
	Entry.Id = LogId;
	Entry.Message = Message;
	Entry.Color = Color;
	Entry.RemainingTime = Duration;
	Entries.Add(Entry);

	// 超上限：FIFO 弹掉最早的（先广播移除，再广播新添加）
	while (Entries.Num() > MaxEntries)
	{
		const int32 OldId = Entries[0].Id;
		Entries.RemoveAt(0);
		RemoveLogInternal(OldId);
	}

	// 广播添加
	OnLogAdded.Broadcast(LogId, Message, Color);

	return LogId;
}

bool UClcLogToastSubsystem::RemoveLog(int32 LogId)
{
	const int32 Removed = Entries.RemoveAll([&](const FLogEntry& E) { return E.Id == LogId; });
	if (Removed > 0)
	{
		RemoveLogInternal(LogId);
		return true;
	}
	return false;
}

void UClcLogToastSubsystem::OnTick()
{
	const float DeltaTime = 0.1f;

	// 倒计时 + 收集过期
	TArray<int32> ExpiredIds;
	for (FLogEntry& E : Entries)
	{
		E.RemainingTime -= DeltaTime;
		if (E.RemainingTime <= 0.0f)
		{
			ExpiredIds.Add(E.Id);
		}
	}

	// 移除过期
	for (int32 Id : ExpiredIds)
	{
		Entries.RemoveAll([&](const FLogEntry& E) { return E.Id == Id; });
		RemoveLogInternal(Id);
	}
}

void UClcLogToastSubsystem::RemoveLogInternal(int32 LogId)
{
	OnLogRemoved.Broadcast(LogId);
}

void UClcLogToastSubsystem::CreateListWidget()
{
	if (!ListWidgetClass)
	{
		ListWidgetClass = LoadClass<UClcLogToastListWidget>(nullptr,
			TEXT("/Game/JadeBetting/UI/WBP_LogToastList.WBP_LogToastList_C"));
	}
	if (!ListWidgetClass)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcLogToast] ListWidgetClass not configured and WBP_LogToastList not found at /Game/JadeBetting/UI/. AddLog will still broadcast delegates but no UI will show."));
		return;
	}

	APlayerController* PC = GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
	if (!PC) return;

	ListWidget = CreateWidget<UClcLogToastListWidget>(PC, ListWidgetClass);
	if (ListWidget)
	{
		ListWidget->AddToViewport(100);
	}
}
