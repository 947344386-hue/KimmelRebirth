// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/Teleport/ClcTeleportSubsystem.h"
#include "UI/ClcUILayers.h"
#include "Tools/Teleport/ClcTeleportPoint.h"
#include "Tools/Teleport/ClcTeleportVolume.h"
#include "Tools/Teleport/UI/ClcTeleportMenuWidget.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"

void UClcTeleportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TeleportMenuAction = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/Tool/Teleport/Input/IA_TeleportMenu.IA_TeleportMenu"));
}

void UClcTeleportSubsystem::Deinitialize()
{
	if (TeleportPromptHandle != 0)
	{
		if (UClcKeyPromptSubsystem* KP = GetLocalPlayer()
			? GetLocalPlayer()->GetSubsystem<UClcKeyPromptSubsystem>() : nullptr)
		{
			KP->UnregisterKeyPrompt(TeleportPromptHandle);
		}
		TeleportPromptHandle = 0;
	}
	CloseMenu();
	RemoveInputBinding();
	ActiveVolumes.Reset();
	CurrentVolume.Reset();
	Super::Deinitialize();
}

void UClcTeleportSubsystem::RegisterVolume(AClcTeleportVolume* Volume)
{
	if (!IsValid(Volume))
	{
		return;
	}

	ActiveVolumes.RemoveAll([Volume](const FActiveVolume& Entry)
	{
		return !Entry.Volume.IsValid() || Entry.Volume.Get() == Volume;
	});

	FActiveVolume& Entry = ActiveVolumes.AddDefaulted_GetRef();
	Entry.Volume = Volume;
	Entry.EnterSequence = ++NextEnterSequence;
	RecomputeCurrentVolume();
	EnsureInputBinding();
}

void UClcTeleportSubsystem::UnregisterVolume(AClcTeleportVolume* Volume)
{
	if (OpenedFromVolume.Get() == Volume)
	{
		CloseMenu();
	}

	ActiveVolumes.RemoveAll([Volume](const FActiveVolume& Entry)
	{
		return !Entry.Volume.IsValid() || Entry.Volume.Get() == Volume;
	});
	RecomputeCurrentVolume();
}

void UClcTeleportSubsystem::ToggleMenu()
{
	if (bMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

bool UClcTeleportSubsystem::OpenMenu()
{
	if (!CanOpenMenu())
	{
		return false;
	}

	APlayerController* PlayerController = GetPlayerController();
	AClcTeleportVolume* Volume = CurrentVolume.Get();
	if (!PlayerController || !Volume)
	{
		return false;
	}

	TSubclassOf<UClcTeleportMenuWidget> WidgetClass = Volume->GetMenuWidgetClass();
	if (!WidgetClass)
	{
		WidgetClass = UClcTeleportMenuWidget::StaticClass();
	}

	MenuWidget = CreateWidget<UClcTeleportMenuWidget>(PlayerController, WidgetClass);
	if (!MenuWidget)
	{
		return false;
	}

	OpenedFromVolume = Volume;
	MenuWidget->InitializeMenu(this, Volume->GetMenuTitle(), Volume->GetValidDestinations());
	MenuWidget->AddToViewport(FClcUIZOrder::OverlayPanel);

	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
	bOwnsInputState = true;
	PlayerController->bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	MenuWidget->SetKeyboardFocus();

	bMenuOpen = true;
	return true;
}

void UClcTeleportSubsystem::CloseMenu()
{
	if (MenuWidget)
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		if (bOwnsInputState)
		{
			PlayerController->SetIgnoreMoveInput(false);
			PlayerController->SetIgnoreLookInput(false);
		}
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}

	bOwnsInputState = false;
	bMenuOpen = false;
	OpenedFromVolume.Reset();
}

bool UClcTeleportSubsystem::RequestTeleport(AClcTeleportPoint* Destination)
{
	APlayerController* PlayerController = GetPlayerController();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!bMenuOpen || !IsValid(Destination) || !Destination->IsEnabled() || !Pawn
		|| Destination->GetWorld() != Pawn->GetWorld())
	{
		return false;
	}

	FVector DesiredLocation = Destination->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		DesiredLocation.Z += Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	DesiredLocation.Z += Destination->GetArrivalClearance();

	FRotator DesiredRotation(0.0f, Destination->GetActorRotation().Yaw, 0.0f);
	FVector SafeLocation = DesiredLocation;
	if (!Pawn->GetWorld()->FindTeleportSpot(Pawn, SafeLocation, DesiredRotation))
	{
		ShowBlockedMessage();
		return false;
	}

	PlayerController->StopMovement();
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->ClearAccumulatedForces();
		}
	}

	if (!Pawn->TeleportTo(SafeLocation, DesiredRotation, false, false))
	{
		ShowBlockedMessage();
		return false;
	}

	const FRotator PreviousControlRotation = PlayerController->GetControlRotation();
	PlayerController->SetControlRotation(FRotator(PreviousControlRotation.Pitch, DesiredRotation.Yaw, 0.0f));
	CloseMenu();
	return true;
}

APlayerController* UClcTeleportSubsystem::GetPlayerController() const
{
	return GetLocalPlayer() ? GetLocalPlayer()->GetPlayerController(GetWorld()) : nullptr;
}

bool UClcTeleportSubsystem::CanOpenMenu() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return !bMenuOpen
		&& CurrentVolume.IsValid()
		&& PlayerController
		&& PlayerController->GetPawn()
		&& !PlayerController->bShowMouseCursor
		&& !PlayerController->IsMoveInputIgnored()
		&& !PlayerController->IsLookInputIgnored();
}

void UClcTeleportSubsystem::RecomputeCurrentVolume()
{
	ActiveVolumes.RemoveAll([](const FActiveVolume& Entry)
	{
		return !Entry.Volume.IsValid();
	});

	AClcTeleportVolume* BestVolume = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Lowest();
	uint64 BestSequence = 0;

	for (const FActiveVolume& Entry : ActiveVolumes)
	{
		AClcTeleportVolume* Volume = Entry.Volume.Get();
		if (!Volume)
		{
			continue;
		}

		const int32 Priority = Volume->GetActivationPriority();
		if (!BestVolume || Priority > BestPriority || (Priority == BestPriority && Entry.EnterSequence > BestSequence))
		{
			BestVolume = Volume;
			BestPriority = Priority;
			BestSequence = Entry.EnterSequence;
		}
	}

	CurrentVolume = BestVolume;

	// ---- 按键提示：进入/离开/切换传送范围时注册/注销/更新 T ----
	if (UClcKeyPromptSubsystem* KP = GetLocalPlayer()
		? GetLocalPlayer()->GetSubsystem<UClcKeyPromptSubsystem>() : nullptr)
	{
		if (CurrentVolume.IsValid())
		{
			if (TeleportPromptHandle == 0)
			{
				TeleportPromptHandle = KP->RegisterKeyPrompt(
					EKeys::T,
					CurrentVolume->GetPromptText(),
					FName("Teleport"), 200);
			}
			else
			{
				KP->UpdateKeyPromptLabel(TeleportPromptHandle, CurrentVolume->GetPromptText());
			}
		}
		else if (TeleportPromptHandle != 0)
		{
			KP->UnregisterKeyPrompt(TeleportPromptHandle);
			TeleportPromptHandle = 0;
		}
	}
}

void UClcTeleportSubsystem::EnsureInputBinding()
{
	APlayerController* PlayerController = GetPlayerController();
	UEnhancedInputComponent* InputComponent = PlayerController
		? Cast<UEnhancedInputComponent>(PlayerController->InputComponent)
		: nullptr;
	if (!InputComponent || !TeleportMenuAction || BoundInputComponent.Get() == InputComponent)
	{
		return;
	}

	RemoveInputBinding();
	FEnhancedInputActionEventBinding& Binding = InputComponent->BindAction(
		TeleportMenuAction, ETriggerEvent::Started, this, &UClcTeleportSubsystem::ToggleMenu);
	InputBindingHandle = Binding.GetHandle();
	BoundInputComponent = InputComponent;
}

void UClcTeleportSubsystem::RemoveInputBinding()
{
	if (UEnhancedInputComponent* InputComponent = BoundInputComponent.Get())
	{
		if (InputBindingHandle != 0)
		{
			InputComponent->RemoveBindingByHandle(InputBindingHandle);
		}
	}
	BoundInputComponent.Reset();
	InputBindingHandle = 0;
}

void UClcTeleportSubsystem::ShowBlockedMessage() const
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UClcLogToastSubsystem* LogToast = LocalPlayer->GetSubsystem<UClcLogToastSubsystem>())
		{
			LogToast->AddLog(TEXT("目标位置被阻挡"), 2.0f, FLinearColor::Yellow);
		}
	}
}
