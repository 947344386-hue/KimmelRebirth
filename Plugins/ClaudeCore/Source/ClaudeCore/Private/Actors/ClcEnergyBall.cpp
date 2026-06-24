// Copyright ClaudeCore. All Rights Reserved.
#include "Actors/ClcEnergyBall.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
AClcEnergyBall::AClcEnergyBall() {
	PrimaryActorTick.bCanEverTick=false;
	BallMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	RootComponent=BallMesh;
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetCastShadow(false); BallMesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M(TEXT("/Engine/BasicShapes/Sphere"));
	if (M.Succeeded()) BallMesh->SetStaticMesh(M.Object);
}
void AClcEnergyBall::BeginPlay() {
	Super::BeginPlay();
	if (UMaterialInterface* Mat=LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/JadeBetting/Materials/M_EnergyBall")))
		BallMesh->SetMaterial(0, Mat);
}
void AClcEnergyBall::SetScaleValue(float V) { CurrentScale=V; SetActorScale3D(FVector(V)); BallMesh->SetVisibility(true); }