// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcPauseMenuSubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "UI/ClcPauseMenuWidget.h"
#include "ClcGameInstance.h"
#include "ClcLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Containers/Ticker.h"

void UClcPauseMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] Initialize"));

	// Load InputAction asset (user must create /Game/JadeBetting/Input/IA_PauseMenu)
	PauseMenuAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/JadeBetting/Input/IA_PauseMenu.IA_PauseMenu"));
	if (!PauseMenuAction)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcPauseMenu] IA_PauseMenu not found — Esc will be handled via FTSTicker polling"));
	}

	// FTSTicker 持续运行，探测 InputComponent 变化（关卡转换后会换新的）。
	// 成本极低：每帧一次 pointer cast + pointer compare，绑上后直接 return。
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			EnsureInputBinding();
			return true;
		}));

	// 注册 Esc 暂停提示到左下角 KeyPrompt UI
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UClcKeyPromptSubsystem* KP = LP->GetSubsystem<UClcKeyPromptSubsystem>())
		{
			KP->RegisterKeyPrompt(EKeys::Escape,
				NSLOCTEXT("ClcPause", "EscPrompt", "暂停"),
				FName("System"), 999);
		}
	}
}

void UClcPauseMenuSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	CloseMenu();
	RemoveInputBinding();
	Super::Deinitialize();
}

// ---- Input Binding ----

void UClcPauseMenuSubsystem::EnsureInputBinding()
{
	APlayerController* PC = GetPlayerController();
	UEnhancedInputComponent* IC = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	if (!IC || !PauseMenuAction || BoundInputComponent.Get() == IC) return;

	RemoveInputBinding();
	FEnhancedInputActionEventBinding& Binding = IC->BindAction(
		PauseMenuAction, ETriggerEvent::Started, this, &UClcPauseMenuSubsystem::ToggleMenu);
	InputBindingHandle = Binding.GetHandle();
	BoundInputComponent = IC;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] Input bound"));
}

void UClcPauseMenuSubsystem::RefreshInputBinding()
{
	// 关卡转换后 GameInstance 调用。强制清除旧绑定，Ticker 下一帧自动重绑。
	BoundInputComponent.Reset();
	InputBindingHandle = 0;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] RefreshInputBinding — forcing rebind next tick"));
}

void UClcPauseMenuSubsystem::RemoveInputBinding()
{
	if (UEnhancedInputComponent* IC = BoundInputComponent.Get())
	{
		if (InputBindingHandle != 0)
			IC->RemoveBindingByHandle(InputBindingHandle);
	}
	BoundInputComponent.Reset();
	InputBindingHandle = 0;
}

// ---- Menu Lifecycle ----

bool UClcPauseMenuSubsystem::CanOpenMenu() const
{
	APlayerController* PC = GetPlayerController();
	return !bMenuOpen && PC && PC->GetPawn()
		&& !PC->bShowMouseCursor
		&& !PC->IsMoveInputIgnored()
		&& !PC->IsLookInputIgnored();
}

void UClcPauseMenuSubsystem::ToggleMenu()
{
	if (bMenuOpen) CloseMenu();
	else OpenMenu();
}

bool UClcPauseMenuSubsystem::OpenMenu()
{
	// 每次打开前确保绑定有效（关卡转换后 InputComponent 可能已重建）
	EnsureInputBinding();

	if (!CanOpenMenu()) return false;

	APlayerController* PC = GetPlayerController();
	if (!PC) return false;

	if (!MenuWidgetClass)
	{
		UClass* BPClass = LoadClass<UClcPauseMenuWidget>(
			nullptr, TEXT("/Game/JadeBetting/UI/WBP_PauseMenu.WBP_PauseMenu_C"));
		MenuWidgetClass = BPClass ? BPClass : UClcPauseMenuWidget::StaticClass();
	}

	MenuWidget = CreateWidget<UClcPauseMenuWidget>(PC, MenuWidgetClass);
	if (!MenuWidget) return false;

	MenuWidget->SetOwningSubsystem(this);
	MenuWidget->AddToViewport(150);
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] Widget added to viewport, bIsFocusable=%d, Visibility=%d, IsInViewport=%d"),
		(int32)MenuWidget->IsFocusable(), (int32)MenuWidget->GetVisibility(), (int32)MenuWidget->IsInViewport());

	// 先不暂停游戏，确保 UI 能渲染出来
	// UGameplayStatics::SetGamePaused(PC, true);

	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	bOwnsInputState = true;
	PC->bShowMouseCursor = true;

	// GameAndUI 模式：UI 接收输入 + 保留编辑器快捷键（F11 等），角色输入由 SetIgnoreMoveInput 屏蔽
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(InputMode);
	// 设键盘焦点：菜单打开时接收 Esc 关闭事件，NativeOnKeyDown 才能触发
	MenuWidget->SetKeyboardFocus();

	bMenuOpen = true;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] Opened —— GamePaused=SKIPPED, InputMode=UIOnly, MouseCursor=%d"), (int32)PC->bShowMouseCursor);
	return true;
}

void UClcPauseMenuSubsystem::CloseMenu()
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	if (APlayerController* PC = GetPlayerController())
	{
		if (bOwnsInputState)
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
		}
		// UGameplayStatics::SetGamePaused(PC, false);
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}

	bOwnsInputState = false;
	bMenuOpen = false;
	UE_LOG(LogClaudeCore, Log, TEXT("[ClcPauseMenu] Closed"));
}

// ---- Button Callbacks ----

void UClcPauseMenuSubsystem::ResumeGame() { CloseMenu(); }

void UClcPauseMenuSubsystem::ManualSave()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;
	UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>();
	if (!SM) return;

	if (SM->SaveGame(UClcSaveManagerSubsystem::AutoSaveSlotName))
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
			if (UClcLogToastSubsystem* Toast = LP->GetSubsystem<UClcLogToastSubsystem>())
				Toast->AddLog(TEXT("游戏已保存"), 2.0f, FLinearColor(0.2f, 0.8f, 0.2f));
	}
}

void UClcPauseMenuSubsystem::LoadSave()
{
	UWorld* World = GetWorld();
	if (!World) return;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return;
	UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>();
	if (!SM || !SM->HasAnySave())
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
			if (UClcLogToastSubsystem* Toast = LP->GetSubsystem<UClcLogToastSubsystem>())
				Toast->AddLog(TEXT("没有可用的存档"), 3.0f, FLinearColor(0.9f, 0.7f, 0.1f));
		return;
	}
	CloseMenu();
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
		ClcGI->LoadAndResumeGame(UClcSaveManagerSubsystem::AutoSaveSlotName);
}

void UClcPauseMenuSubsystem::GoToMainMenu()
{
	UWorld* World = GetWorld();
	if (!World) return;
	CloseMenu();
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(World->GetGameInstance()))
		ClcGI->GoToMainMenu(true);
}

void UClcPauseMenuSubsystem::QuitGame()
{
	UWorld* World = GetWorld();
	if (!World) return;
	CloseMenu();
	if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(World->GetGameInstance()))
		ClcGI->RequestQuit();
}

APlayerController* UClcPauseMenuSubsystem::GetPlayerController() const
{
	return GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
}
