// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "UI/ClcKeyPromptWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UClcKeyPromptSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 不在此创建 Widget 或启 Timer——有活跃 prompt 时按需创建/启动。
}

void UClcKeyPromptSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GateTickHandle);
	}
	if (PromptWidget)
	{
		PromptWidget->RemoveFromParent();
		PromptWidget = nullptr;
	}
	ActivePrompts.Reset();
	HandleToKey.Reset();
	NextHandle = 0;
	Super::Deinitialize();
}

int32 UClcKeyPromptSubsystem::RegisterKeyPrompt(const FKey& Key, const FText& Label,
	FName Category, int32 SortPriority)
{
	if (!Key.IsValid())
	{
		return 0;
	}

	const int32 Handle = ++NextHandle;
	HandleToKey.Add(Handle, Key);

	if (FPromptEntry* Existing = ActivePrompts.FindByPredicate(
		[&Key](const FPromptEntry& Entry) { return Entry.Prompt.Key == Key; }))
	{
		Existing->RefCount++;
		// label 不覆盖——首次注册者的文案生效（去重语义）。
	}
	else
	{
		FPromptEntry& NewEntry = ActivePrompts.AddDefaulted_GetRef();
		NewEntry.Prompt.Key = Key;
		NewEntry.Prompt.Label = Label;
		NewEntry.Prompt.Category = Category;
		NewEntry.Prompt.SortPriority = SortPriority;
		NewEntry.RefCount = 1;
		NewEntry.Handle = Handle;
	}

	RebuildPromptWidget();
	return Handle;
}

void UClcKeyPromptSubsystem::UnregisterKeyPrompt(int32 Handle)
{
	if (Handle == 0)
	{
		return;
	}

	const FKey* FoundKey = HandleToKey.Find(Handle);
	if (!FoundKey)
	{
		return;
	}

	HandleToKey.Remove(Handle);

	const int32 Idx = ActivePrompts.IndexOfByPredicate(
		[FoundKey](const FPromptEntry& Entry) { return Entry.Prompt.Key == *FoundKey; });
	if (Idx != INDEX_NONE)
	{
		if (--ActivePrompts[Idx].RefCount <= 0)
		{
			ActivePrompts.RemoveAt(Idx);
		}
	}

	RebuildPromptWidget();
}

void UClcKeyPromptSubsystem::UpdateKeyPromptLabel(int32 Handle, const FText& Label)
{
	const FKey* FoundKey = HandleToKey.Find(Handle);
	if (!FoundKey)
	{
		return;
	}

	if (FPromptEntry* Entry = ActivePrompts.FindByPredicate(
		[FoundKey](const FPromptEntry& E) { return E.Prompt.Key == *FoundKey; }))
	{
		Entry->Prompt.Label = Label;
		RebuildPromptWidget();
	}
}

bool UClcKeyPromptSubsystem::IsPromptRegistered(int32 Handle) const
{
	return Handle != 0 && HandleToKey.Contains(Handle);
}

APlayerController* UClcKeyPromptSubsystem::GetPlayerController() const
{
	return GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
}

bool UClcKeyPromptSubsystem::ShouldShowPrompts() const
{
	const APlayerController* PC = GetPlayerController();
	return PC
		&& PC->GetPawn()
		&& !PC->bShowMouseCursor
		&& !PC->IsMoveInputIgnored()
		&& !PC->IsLookInputIgnored();
}

TArray<FClcKeyPrompt> UClcKeyPromptSubsystem::GetSortedPrompts() const
{
	TArray<FClcKeyPrompt> Sorted;
	Sorted.Reserve(ActivePrompts.Num());
	for (const FPromptEntry& E : ActivePrompts)
	{
		Sorted.Add(E.Prompt);
	}
	Sorted.StableSort([](const FClcKeyPrompt& A, const FClcKeyPrompt& B)
	{
		return A.SortPriority < B.SortPriority;
	});
	return Sorted;
}

void UClcKeyPromptSubsystem::RebuildPromptWidget()
{
	if (ActivePrompts.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(GateTickHandle);
		}
		if (PromptWidget)
		{
			PromptWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (!GateTickHandle.IsValid())
		{
			World->GetTimerManager().SetTimer(
				GateTickHandle, this, &UClcKeyPromptSubsystem::OnGateTick, 0.1f, true);
		}
	}

	if (ShouldShowPrompts())
	{
		if (!PromptWidget)
		{
			CreatePromptWidget();
		}
		if (PromptWidget)
		{
			PromptWidget->Refresh(GetSortedPrompts());
			PromptWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (PromptWidget)
	{
		PromptWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UClcKeyPromptSubsystem::OnGateTick()
{
	if (ActivePrompts.Num() == 0)
	{
		return;
	}

	if (ShouldShowPrompts())
	{
		// 懒创建：Initialize 阶段注册的提示（如背包 B）此时 PC 可能还没就绪，
		// Widget 不会在 RebuildPromptWidget 里创建；闸门打开后在这里补创建。
		if (!PromptWidget)
		{
			CreatePromptWidget();
		}
		if (PromptWidget && PromptWidget->GetVisibility() != ESlateVisibility::Visible)
		{
			PromptWidget->Refresh(GetSortedPrompts());
			PromptWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (PromptWidget && PromptWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		PromptWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UClcKeyPromptSubsystem::CreatePromptWidget()
{
	if (!PromptWidgetClass)
	{
		PromptWidgetClass = LoadClass<UClcKeyPromptWidget>(nullptr,
			TEXT("/Game/JadeBetting/UI/WBP_KeyPrompt.WBP_KeyPrompt_C"));
	}
	if (!PromptWidgetClass)
	{
		PromptWidgetClass = UClcKeyPromptWidget::StaticClass();
	}

	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return;
	}

	PromptWidget = CreateWidget<UClcKeyPromptWidget>(PC, PromptWidgetClass);
	if (PromptWidget)
	{
		PromptWidget->AddToViewport(110); // 低于传送菜单(120)，高于背包(100)
	}
}
