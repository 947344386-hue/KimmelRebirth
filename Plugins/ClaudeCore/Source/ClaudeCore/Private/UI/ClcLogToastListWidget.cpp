// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcLogToastListWidget.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

void UClcLogToastListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 自动绑 Subsystem 的委托
	if (ULocalPlayer* LP = GetOwningLocalPlayer())
	{
		if (UClcLogToastSubsystem* Sub = LP->GetSubsystem<UClcLogToastSubsystem>())
		{
			Sub->OnLogAdded.AddDynamic(this, &UClcLogToastListWidget::HandleLogAdded);
			Sub->OnLogRemoved.AddDynamic(this, &UClcLogToastListWidget::HandleLogRemoved);
		}
	}
}

void UClcLogToastListWidget::HandleLogAdded(int32 LogId, const FString& Message, const FLinearColor& Color)
{
	// 调 BP 实现创建 entry widget
	UUserWidget* Entry = CreateEntryWidget(Message, Color);
	if (!Entry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcLogToastList] CreateEntryWidget returned null! LogId=%d"), LogId);
		return;
	}

	// 插入容器顶部（新 log 在顶部，老 log 往下顶）
	if (LogContainer)
	{
		LogContainer->InsertChildAt(0, Entry);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcLogToastList] LogContainer is null! BindWidget 名字必须是 'LogContainer'。"));
	}

	EntryMap.Add(LogId, Entry);
}

void UClcLogToastListWidget::HandleLogRemoved(int32 LogId)
{
	if (UUserWidget* const* Found = EntryMap.Find(LogId))
	{
		if (UUserWidget* Entry = *Found)
		{
			Entry->RemoveFromParent();
		}
		EntryMap.Remove(LogId);
	}
}
