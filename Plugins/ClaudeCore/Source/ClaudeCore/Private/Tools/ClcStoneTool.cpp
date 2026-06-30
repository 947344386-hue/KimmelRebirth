// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcStoneTool.h"
#include "Components/StaticMeshComponent.h"
#include "Actors/ClcOpeningStone.h"

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
	const float FPS = 60.0f;
	const float AdjustedDelta = DeltaTime * FPS; // 帧率无关
	const float T_Location = 1.0f - FMath::Pow(1.0f - SmoothSpeedLocation, AdjustedDelta);
	const float T_Rotation = 1.0f - FMath::Pow(1.0f - SmoothSpeedRotation, AdjustedDelta);

	SetActorLocation(FMath::Lerp(GetActorLocation(), TargetLocation, T_Location));
	SetActorRotation(FMath::Lerp(GetActorRotation(), TargetRotation, T_Rotation));
}

void AClcStoneTool::Initialize(AClcOpeningStone* Stone)
{
	TargetStone = Stone;
	// Spawn 后 BP 配置已加载，用 MaxDurability 重置当前耐久度
	// （构造函数赋值时 MaxDurability 还是 C++ 默认值，BP 配的值要等序列化后才生效）
	CurrentDurability = MaxDurability;
}

void AClcStoneTool::ConsumeDurability(float Amount)
{
	CurrentDurability = FMath::Max(0.0f, CurrentDurability - Amount);
}
