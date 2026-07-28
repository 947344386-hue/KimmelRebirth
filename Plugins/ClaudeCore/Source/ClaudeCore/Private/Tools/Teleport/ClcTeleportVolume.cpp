// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/Teleport/ClcTeleportVolume.h"
#include "Tools/Teleport/ClcTeleportPoint.h"
#include "Tools/Teleport/ClcTeleportSubsystem.h"
#include "Tools/Teleport/UI/ClcTeleportMenuWidget.h"
#include "Components/BoxComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AClcTeleportVolume::AClcTeleportVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(200.0f, 200.0f, 150.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);

	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AClcTeleportVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.AddDynamic(this, &AClcTeleportVolume::OnTriggerEndOverlap);

	MenuTitle = NSLOCTEXT("ClcTeleport", "DefaultMenuTitle", "选择传送点");
	MenuWidgetClass = UClcTeleportMenuWidget::StaticClass();
	PromptText = NSLOCTEXT("ClcTeleport", "DefaultPromptText", "按 T 打开传送列表");
}

void AClcTeleportVolume::BeginPlay()
{
	Super::BeginPlay();

	// 玩家可能在体积内出生——BeginOverlap 不对"开局已重叠"的 Actor 触发。
	// 短轮询补注册：抓到本地玩家或超时即停（Pawn 可能晚于体积 BeginPlay 生成 / Possess 有延迟）。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InitialOverlapTimer, this, &AClcTeleportVolume::ScanInitialOverlap, 0.2f, true);
	}
}

void AClcTeleportVolume::ScanInitialOverlap()
{
	TArray<AActor*> Overlapping;
	TriggerBox->GetOverlappingActors(Overlapping, APawn::StaticClass());

	bool bRegisteredAny = false;
	for (AActor* Actor : Overlapping)
	{
		if (ULocalPlayer* LP = GetLocalPlayerForActor(Actor))
		{
			RegisterForPlayer(LP);
			bRegisteredAny = true;
		}
	}

	++InitialOverlapAttempts;
	if (bRegisteredAny || InitialOverlapAttempts >= 25) // ~5s @ 0.2s
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InitialOverlapTimer);
		}
	}
}

void AClcTeleportVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitialOverlapTimer);
	}

	for (const TWeakObjectPtr<ULocalPlayer>& Player : RegisteredPlayers)
	{
		if (ULocalPlayer* LocalPlayer = Player.Get())
		{
			if (UClcTeleportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UClcTeleportSubsystem>())
			{
				Subsystem->UnregisterVolume(this);
			}
		}
	}
	RegisteredPlayers.Reset();

	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &AClcTeleportVolume::OnTriggerBeginOverlap);
	TriggerBox->OnComponentEndOverlap.RemoveDynamic(this, &AClcTeleportVolume::OnTriggerEndOverlap);
	Super::EndPlay(EndPlayReason);
}

TArray<AClcTeleportPoint*> AClcTeleportVolume::GetValidDestinations() const
{
	TArray<AClcTeleportPoint*> Result;
	TSet<AClcTeleportPoint*> Seen;

	for (AClcTeleportPoint* Destination : Destinations)
	{
		if (IsValid(Destination) && Destination->IsEnabled() && !Seen.Contains(Destination))
		{
			Seen.Add(Destination);
			Result.Add(Destination);
		}
	}
	return Result;
}

void AClcTeleportVolume::OnTriggerBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayerForActor(OtherActor))
	{
		RegisterForPlayer(LocalPlayer);
	}
}

void AClcTeleportVolume::OnTriggerEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayerForActor(OtherActor))
	{
		UnregisterForPlayer(LocalPlayer);
	}
}

ULocalPlayer* AClcTeleportVolume::GetLocalPlayerForActor(AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return nullptr;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	return PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
}

void AClcTeleportVolume::RegisterForPlayer(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return;
	}

	if (UClcTeleportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UClcTeleportSubsystem>())
	{
		Subsystem->RegisterVolume(this);
		RegisteredPlayers.AddUnique(LocalPlayer);
	}
}

void AClcTeleportVolume::UnregisterForPlayer(ULocalPlayer* LocalPlayer)
{
	if (!LocalPlayer)
	{
		return;
	}

	if (UClcTeleportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UClcTeleportSubsystem>())
	{
		Subsystem->UnregisterVolume(this);
	}
	RegisteredPlayers.Remove(LocalPlayer);
}
