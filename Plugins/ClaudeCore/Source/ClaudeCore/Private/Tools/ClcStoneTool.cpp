// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcStoneTool.h"
#include "ClcLog.h"
#include "Components/StaticMeshComponent.h"
#include "Actors/ClcOpeningStone.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

AClcStoneTool::AClcStoneTool()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	ToolRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ToolRoot"));
	RootComponent = ToolRoot;

	ToolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(ToolRoot);
	ToolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ToolMesh->SetCastShadow(false);

	CurrentDurability = MaxDurability;
}

void AClcStoneTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHasTarget) return;

	// 指数平滑：Target = 目标，Current = 当前，Speed = 追赶速率
	const FVector CurLoc = GetActorLocation();
	const FRotator CurRot = GetActorRotation();

	if (CurLoc.Equals(TargetLocation, 0.01f) && CurRot.Equals(TargetRotation, 0.01f))
	{
		return;
	}

	const float FPS = 60.0f;
	const float AdjustedDelta = DeltaTime * FPS;
	const float T_Location = 1.0f - FMath::Pow(1.0f - SmoothSpeedLocation, AdjustedDelta);
	const float T_Rotation = 1.0f - FMath::Pow(1.0f - SmoothSpeedRotation, AdjustedDelta);

	SetActorLocation(FMath::Lerp(CurLoc, TargetLocation, T_Location));
	SetActorRotation(FMath::Lerp(CurRot, TargetRotation, T_Rotation));
}

void AClcStoneTool::Initialize(AClcOpeningStone* Stone)
{
	if (!Stone)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcStoneTool] Initialize called with null Stone!"));
		return;
	}
	TargetStone = Stone;

	// 注册本工具的最大耐久（BP 可配），并读取持久化耐久
	if (ToolType != EClcRepairableTool::None)
	{
		if (UClcToolDurabilitySubsystem* DuraSys = UClcToolDurabilitySubsystem::Get(GetWorld()))
		{
			DuraSys->InitTool(ToolType, MaxDurability);
			CurrentDurability = DuraSys->GetDurability(ToolType);
		}
		else
		{
			CurrentDurability = MaxDurability;
		}
	}
	else
	{
		CurrentDurability = MaxDurability;
	}
}

void AClcStoneTool::ConsumeDurability(float Amount)
{
	CurrentDurability = FMath::Max(0.0f, CurrentDurability - Amount);

	// 写回持久化子系统
	if (ToolType != EClcRepairableTool::None)
	{
		if (UClcToolDurabilitySubsystem* DuraSys = UClcToolDurabilitySubsystem::Get(GetWorld()))
		{
			DuraSys->SetDurability(ToolType, CurrentDurability);
		}
	}
}

void AClcStoneTool::RestoreFullDurability()
{
	CurrentDurability = MaxDurability;

	if (ToolType != EClcRepairableTool::None)
	{
		if (UClcToolDurabilitySubsystem* DuraSys = UClcToolDurabilitySubsystem::Get(GetWorld()))
		{
			DuraSys->SetDurability(ToolType, MaxDurability);
		}
	}
}
