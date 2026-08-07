// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcFacilityManager.h"
#include "Actors/ClcJadeWorkbench.h"
#include "Actors/ClcCuttingTable.h"
#include "Components/ArrowComponent.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Engine/World.h"

AClcFacilityManager::AClcFacilityManager()
{
	PrimaryActorTick.bCanEverTick = false;

	ManagerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ManagerRoot"));
	RootComponent = ManagerRoot;

	SoloWorkbenchPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SoloWorkbenchPoint"));
	SoloWorkbenchPoint->SetupAttachment(ManagerRoot);
	SoloWorkbenchPoint->ArrowColor = FColor(0, 200, 255);
	SoloWorkbenchPoint->ArrowSize = 1.5f;
	SoloWorkbenchPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	PairedWorkbenchPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("PairedWorkbenchPoint"));
	PairedWorkbenchPoint->SetupAttachment(ManagerRoot);
	PairedWorkbenchPoint->ArrowColor = FColor(0, 255, 128);
	PairedWorkbenchPoint->ArrowSize = 1.5f;
	PairedWorkbenchPoint->SetRelativeLocation(FVector(-250.0f, 0.0f, 0.0f));

	PairedCuttingTablePoint = CreateDefaultSubobject<UArrowComponent>(TEXT("PairedCuttingTablePoint"));
	PairedCuttingTablePoint->SetupAttachment(ManagerRoot);
	PairedCuttingTablePoint->ArrowColor = FColor(255, 128, 0);
	PairedCuttingTablePoint->ArrowSize = 1.5f;
	PairedCuttingTablePoint->SetRelativeLocation(FVector(250.0f, 0.0f, 0.0f));
}

void AClcFacilityManager::BeginPlay()
{
	Super::BeginPlay();
#if WITH_EDITOR
	DestroyPreviewActors();
#endif
	ApplySoloLayout();
}

void AClcFacilityManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if WITH_EDITOR
	DestroyPreviewActors();
#endif
	Super::EndPlay(EndPlayReason);
}

void AClcFacilityManager::ApplySoloLayout()
{
	DestroyCuttingTable();
	DestroyWorkbench();

	if (!WorkbenchClass) return;

	const FTransform T = SoloWorkbenchPoint->GetComponentTransform();
	SpawnedWorkbench = GetWorld()->SpawnActor<AClcJadeWorkbench>(WorkbenchClass, T);
	bPairedLayout = false;
}

void AClcFacilityManager::ApplyPairedLayout()
{
	DestroyCuttingTable();
	DestroyWorkbench();

	if (WorkbenchClass)
	{
		const FTransform T = PairedWorkbenchPoint->GetComponentTransform();
		SpawnedWorkbench = GetWorld()->SpawnActor<AClcJadeWorkbench>(WorkbenchClass, T);
	}

	if (CuttingTableClass)
	{
		const FTransform T = PairedCuttingTablePoint->GetComponentTransform();
		SpawnedCuttingTable = GetWorld()->SpawnActor<AClcCuttingTable>(CuttingTableClass, T);
	}

	bPairedLayout = true;
}

void AClcFacilityManager::DestroyWorkbench()
{
	if (SpawnedWorkbench)
	{
		SpawnedWorkbench->Destroy();
		SpawnedWorkbench = nullptr;
	}
}

void AClcFacilityManager::DestroyCuttingTable()
{
	if (SpawnedCuttingTable)
	{
		SpawnedCuttingTable->Destroy();
		SpawnedCuttingTable = nullptr;
	}
}

void AClcFacilityManager::RefreshLayout()
{
	if (UClcToolDurabilitySubsystem* Durability = UClcToolDurabilitySubsystem::Get(GetWorld()))
	{
		if (Durability->HasCuttingTable() && !bPairedLayout)
		{
			ApplyPairedLayout();
		}
	}
}

#if WITH_EDITOR

void AClcFacilityManager::ToggleSoloPreview()
{
	if (PreviewMode == EPreviewMode::Solo)
	{
		SavePreviewTransforms();
		DestroyPreviewActors();
		PreviewMode = EPreviewMode::None;
	}
	else
	{
		DestroyPreviewActors();
		SpawnPreviewActors(EPreviewMode::Solo);
		PreviewMode = EPreviewMode::Solo;
	}
}

void AClcFacilityManager::TogglePairedPreview()
{
	if (PreviewMode == EPreviewMode::Paired)
	{
		SavePreviewTransforms();
		DestroyPreviewActors();
		PreviewMode = EPreviewMode::None;
	}
	else
	{
		DestroyPreviewActors();
		SpawnPreviewActors(EPreviewMode::Paired);
		PreviewMode = EPreviewMode::Paired;
	}
}

void AClcFacilityManager::StopPreview()
{
	DestroyPreviewActors();
	PreviewMode = EPreviewMode::None;
}

void AClcFacilityManager::SpawnPreviewActors(EPreviewMode Mode)
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;

	if (Mode == EPreviewMode::Solo && WorkbenchClass)
	{
		PreviewWorkbench = World->SpawnActor<AActor>(WorkbenchClass,
			SoloWorkbenchPoint->GetComponentTransform(), SpawnParams);
	}
	else if (Mode == EPreviewMode::Paired)
	{
		if (WorkbenchClass)
		{
			PreviewWorkbench = World->SpawnActor<AActor>(WorkbenchClass,
				PairedWorkbenchPoint->GetComponentTransform(), SpawnParams);
		}
		if (CuttingTableClass)
		{
			PreviewCuttingTable = World->SpawnActor<AActor>(CuttingTableClass,
				PairedCuttingTablePoint->GetComponentTransform(), SpawnParams);
		}
	}
}

void AClcFacilityManager::SavePreviewTransforms()
{
	if (PreviewMode == EPreviewMode::Solo)
	{
		SetArrowFromActor(SoloWorkbenchPoint, PreviewWorkbench);
	}
	else if (PreviewMode == EPreviewMode::Paired)
	{
		SetArrowFromActor(PairedWorkbenchPoint, PreviewWorkbench);
		SetArrowFromActor(PairedCuttingTablePoint, PreviewCuttingTable);
	}
}

void AClcFacilityManager::DestroyPreviewActors()
{
	if (PreviewWorkbench)
	{
		PreviewWorkbench->Destroy();
		PreviewWorkbench = nullptr;
	}
	if (PreviewCuttingTable)
	{
		PreviewCuttingTable->Destroy();
		PreviewCuttingTable = nullptr;
	}
}

void AClcFacilityManager::SetArrowFromActor(UArrowComponent* Arrow, const AActor* Source)
{
	if (!Arrow || !Source || !ManagerRoot) return;

	const FTransform WorldTransform = Source->GetActorTransform();
	const FTransform RelativeTransform = WorldTransform.GetRelativeTransform(ManagerRoot->GetComponentTransform());
	Arrow->SetRelativeTransform(RelativeTransform);
}

#endif