// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcStoneTool.h"
#include "ClcLog.h"
#include "Components/StaticMeshComponent.h"
#include "Actors/ClcOpeningStone.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

// 工具类型 → 中文名（耐久提示用）
static const TCHAR* GetToolDisplayName(EClcRepairableTool Type)
{
	switch (Type)
	{
	case EClcRepairableTool::Opener:     return TEXT("开窗器");
	case EClcRepairableTool::Flashlight: return TEXT("手电筒");
	case EClcRepairableTool::Combined:   return TEXT("手电开窗器");
	default:                             return TEXT("工具");
	}
}

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

	// 耐久耗尽/低耐久预警飘字（一次性）
	CheckDurabilityAndNotify();
}

void AClcStoneTool::CheckDurabilityAndNotify()
{
	if (!GetWorld()) return;

	// 已损坏 → 飘一次"耗尽"红字
	if (IsBroken())
	{
		if (!bBrokenNotified)
		{
			bBrokenNotified = true;
			bLowDurabilityNotified = true; // 坏了就别再飘低耐久
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (UClcLogToastSubsystem* LT = ClcGetLogToast(PC))
				{
					const TCHAR* ToolName = GetToolDisplayName(ToolType);
					LT->AddLog(FString::Printf(TEXT("%s耐久耗尽，需前往修理站修复"), ToolName), 2.5f, FLinearColor::Red);
				}
			}
		}
		return;
	}

	// 低耐久预警（一次性）
	if (!bLowDurabilityNotified && GetDurabilityRatio() < LowDurabilityThreshold)
	{
		bLowDurabilityNotified = true;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (UClcLogToastSubsystem* LT = ClcGetLogToast(PC))
			{
				const TCHAR* ToolName = GetToolDisplayName(ToolType);
				LT->AddLog(FString::Printf(TEXT("%s耐久不足，即将耗尽"), ToolName), 2.0f, FLinearColor::Yellow);
			}
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
