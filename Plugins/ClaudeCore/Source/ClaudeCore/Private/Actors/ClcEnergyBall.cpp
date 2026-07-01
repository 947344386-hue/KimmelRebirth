// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcEnergyBall.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"

AClcEnergyBall::AClcEnergyBall()
{
	PrimaryActorTick.bCanEverTick = false;

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	RootComponent = BallMesh;

	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetCastShadow(false);
	BallMesh->SetVisibility(false); // 默认不可见，鹰眼激活时才显示

	// 默认球体
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBall(
		TEXT("/Engine/BasicShapes/Sphere"));
	if (DefaultBall.Succeeded())
	{
		BallMesh->SetStaticMesh(DefaultBall.Object);
	}
}

void AClcEnergyBall::BeginPlay()
{
	Super::BeginPlay();

		if (!BallMaterial)
		{
			BallMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/JadeBetting/Materials/M_EnergyBall"));
		}
		if (BallMaterial)
		{
			BallMesh->SetMaterial(0, BallMaterial);
		}
}

void AClcEnergyBall::SetScaleValue(float Scale)
{
	CurrentScale = Scale;
	SetActorScale3D(FVector(Scale));
	BallMesh->SetVisibility(true);
}
