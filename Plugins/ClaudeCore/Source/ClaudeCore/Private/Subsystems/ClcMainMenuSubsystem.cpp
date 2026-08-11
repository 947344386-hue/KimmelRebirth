// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcMainMenuSubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "UI/ClcMainMenuWidget.h"
#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UClcMainMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] Initialize"));

	// 设定默认会话配置
	DefaultSessionConfig.StartingGold = 50000;
	DefaultSessionConfig.Difficulty = EClcDifficultyPreset::Normal;
	DefaultSessionConfig.DifficultyMultiplier = 1.0f;

	// 在下一帧自动显示主菜单——此时 PlayerController 已就位
	// 注意：仅当加载的关卡是主菜单关卡时才显示（玩法关卡不显示菜单）
	// 由关卡的 Level Blueprint 或 GameMode 调用 ShowMainMenu
}

void UClcMainMenuSubsystem::Deinitialize()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] Deinitialize"));

	if (MenuWidget && MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
	}
	MenuWidget = nullptr;
	bMenuVisible = false;

	Super::Deinitialize();
}

void UClcMainMenuSubsystem::ShowMainMenu()
{
	if (bMenuVisible && MenuWidget)
	{
		UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcMainMenu] ShowMainMenu —— 已可见"));
		return;
	}

	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMainMenu] ShowMainMenu —— PlayerController 不可用"));
		return;
	}

	// 懒加载 Widget 类：优先尝试 BP 子类 WBP_MainMenu，不存在则回退 C++ 默认
	if (!MenuWidgetClass)
	{
		UClass* BPClass = LoadClass<UClcMainMenuWidget>(
			nullptr, TEXT("/Game/JadeBetting/UI/WBP_MainMenu.WBP_MainMenu_C"));
		MenuWidgetClass = BPClass ? BPClass : UClcMainMenuWidget::StaticClass();
	}

	MenuWidget = CreateWidget<UClcMainMenuWidget>(PC, MenuWidgetClass);
	if (!MenuWidget)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMainMenu] ShowMainMenu —— CreateWidget 失败"));
		return;
	}

	MenuWidget->InitializeMenu(this);
	MenuWidget->AddToViewport(0);  // ZOrder 0——全屏最底层

	// 切换到纯 UI 输入模式
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	bMenuVisible = true;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] ShowMainMenu —— 菜单已显示"));
}

void UClcMainMenuSubsystem::HideMainMenu()
{
	if (!bMenuVisible || !MenuWidget)
	{
		return;
	}

	if (MenuWidget->IsInViewport())
	{
		MenuWidget->RemoveFromParent();
	}

	// 恢复游戏输入模式
	if (APlayerController* PC = GetPlayerController())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}

	bMenuVisible = false;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] HideMainMenu"));
}

void UClcMainMenuSubsystem::StartNewGame(const FClcSessionConfig& Config)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] StartNewGame —— Gold=%d, Difficulty=%d"),
		Config.StartingGold, static_cast<uint8>(Config.Difficulty));

	HideMainMenu();

	// 委托给 GameInstance——通过 GetWorld() 拿 GameInstance（ULocalPlayerSubsystem 没有直接 GetGameInstance）
	UWorld* World = GetWorld();
	UGameInstance* RawGI = World ? World->GetGameInstance() : nullptr;
	UClcGameInstance* GI = Cast<UClcGameInstance>(RawGI);
	if (GI)
	{
		GI->StartNewGame(Config);
	}
	else
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMainMenu] StartNewGame —— GameInstance 不是 UClcGameInstance！（实际类型: %s）"),
			RawGI ? *RawGI->GetClass()->GetName() : TEXT("null"));
	}
}

void UClcMainMenuSubsystem::ContinueGame(const FString& SlotName)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] ContinueGame —— Slot=%s"), *SlotName);

	HideMainMenu();

		UWorld* World = GetWorld();
		UClcGameInstance* GI = World ? Cast<UClcGameInstance>(World->GetGameInstance()) : nullptr;
		if (GI)
		{
			GI->LoadAndResumeGame(SlotName);
		}
		else
		{
			UE_LOG(LogClaudeCore, Error, TEXT("[ClcMainMenu] ContinueGame —— GameInstance 不是 UClcGameInstance！"));
		}
}

void UClcMainMenuSubsystem::QuitGame()
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] QuitGame"));

		UWorld* World = GetWorld();
		UClcGameInstance* GI = World ? Cast<UClcGameInstance>(World->GetGameInstance()) : nullptr;
		if (GI)
		{
			GI->RequestQuit();
		}
		else
		{
			// 兜底
			if (APlayerController* PC = GetPlayerController())
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
		}
}

TArray<FClcSaveMetaData> UClcMainMenuSubsystem::GetSaveSlots() const
{
	UWorld* W = GetWorld();
	if (W)
	{
		UGameInstance* GI = W->GetGameInstance();
		if (UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>())
		{
			return SM->GetSaveSlots();
		}
	}
	return TArray<FClcSaveMetaData>();
}

bool UClcMainMenuSubsystem::DeleteSave(const FString& SlotName)
{
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcMainMenu] DeleteSave —— Slot=%s"), *SlotName);
	UWorld* W = GetWorld();
	if (W)
	{
		UGameInstance* GI = W->GetGameInstance();
		if (UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>())
		{
			return SM->DeleteSave(SlotName);
		}
	}
	return false;
}

APlayerController* UClcMainMenuSubsystem::GetPlayerController() const
{
	const ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return nullptr;
	}
	return LP->GetPlayerController(GetWorld());
}
