// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcPauseMenuSubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "UI/ClcPauseMenuWidget.h"
#include "UI/ClcSaveSlotListWidget.h"
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
	OpenSlotList(/*bLoadMode=*/false);
}

void UClcPauseMenuSubsystem::LoadSave()
{
	OpenSlotList(/*bLoadMode=*/true);
}

void UClcPauseMenuSubsystem::OpenSlotList(bool bLoadMode)
{
	APlayerController* PC = GetPlayerController();
	if (!PC) return;

	UClcSaveManagerSubsystem* SM = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			SM = GI->GetSubsystem<UClcSaveManagerSubsystem>();
		}
	}
	if (!SM) return;

	// 已有列表则先关（防重入）
	CloseSlotList();

	// 约定路径 WBP 换皮（无 WBP 时回退 C++ 默认布局）
	TSubclassOf<UClcSaveSlotListWidget> SlotListClass =
		LoadClass<UClcSaveSlotListWidget>(nullptr,
			TEXT("/Game/JadeBetting/UI/WBP_SaveSlotList.WBP_SaveSlotList_C"));
	if (!SlotListClass) SlotListClass = UClcSaveSlotListWidget::StaticClass();

	SlotListWidget = CreateWidget<UClcSaveSlotListWidget>(PC, SlotListClass);
	if (!SlotListWidget) return;

	bSlotListLoadMode = bLoadMode;
	SlotListWidget->InitSlotList(SM, bLoadMode ? EClcSaveSlotListMode::Load : EClcSaveSlotListMode::Save);
	SlotListWidget->OnSlotPicked.AddDynamic(this, &UClcPauseMenuSubsystem::HandleSlotPicked);
	SlotListWidget->AddToViewport(160);
	SlotListWidget->SetKeyboardFocus();
}

void UClcPauseMenuSubsystem::HandleSlotPicked(const FString& SlotName)
{
	const bool bWasLoadMode = bSlotListLoadMode;
	CloseSlotList();

	if (SlotName.IsEmpty())
	{
		return; // 玩家点了返回
	}

	// 保存模式：写档已在 Widget 内完成，这里提示即可
	if (!bWasLoadMode)
	{
		if (const ULocalPlayer* LP = GetLocalPlayer())
		{
			if (UClcLogToastSubsystem* Toast = LP->GetSubsystem<UClcLogToastSubsystem>())
			{
				Toast->AddLog(FString::Printf(TEXT("游戏已保存到 %s"), *SlotName),
					2.0f, FLinearColor(0.2f, 0.8f, 0.2f));
			}
		}
		return;
	}

	// 读档模式：关闭暂停菜单并加载所选槽位
	{
		UWorld* World = GetWorld();
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		if (!GI) return;
		CloseMenu();
		if (UClcGameInstance* ClcGI = Cast<UClcGameInstance>(GI))
		{
			ClcGI->LoadAndResumeGame(SlotName);
		}
	}
}

void UClcPauseMenuSubsystem::CloseSlotList()
{
	if (SlotListWidget)
	{
		SlotListWidget->RemoveFromParent();
		SlotListWidget = nullptr;
	}
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
