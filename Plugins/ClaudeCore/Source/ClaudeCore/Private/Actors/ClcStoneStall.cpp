// Copyright ClaudeCore. All Rights Reserved.
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
AClcStoneStall::AClcStoneStall() {
	PrimaryActorTick.bCanEverTick=false;
	StallMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StallMesh"));
	RootComponent=StallMesh;
	StallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DM(TEXT("/Engine/BasicShapes/Cube"));
	if (DM.Succeeded()) { StallMesh->SetStaticMesh(DM.Object); StallMesh->SetRelativeScale3D(FVector(3,1,0.2f)); }
	BallSpawnPoint=CreateDefaultSubobject<USceneComponent>(TEXT("BallSpawnPoint"));
	BallSpawnPoint->SetupAttachment(StallMesh); BallSpawnPoint->SetRelativeLocation(FVector(0,0,200));
	StoneSpawnCenter=CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnCenter"));
	StoneSpawnCenter->SetupAttachment(StallMesh);
}
void AClcStoneStall::BeginPlay() {
	Super::BeginPlay();
	MarketSubsystem=GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>();
	if (MarketSubsystem) MarketSubsystem->RegisterStall(this);
	SpawnStones();
}
void AClcStoneStall::SpawnStones() {
	for (AClcStone* S : SpawnedStones) if (S) S->Destroy();
	SpawnedStones.Empty();
	if (!MarketSubsystem) return;
	UClcStallConfig* SC = MarketSubsystem->GetStallConfig();
	UClcStoneMeshConfig* MC = MarketSubsystem->GetMeshConfig();
	if (!SC||!MC) return;
	for (int i=0;i<SC->StonesPerStall;i++) {
		bool Ok; FClcStoneInternalData D = MarketSubsystem->GenerateStoneInternal(Ok);
		if (!Ok) continue;
		UStaticMesh* M = MC->GetRandomMesh(); if (!M) continue;
		float A=FMath::FRandRange(0,2*PI), Dst=FMath::FRandRange(0.f,SC->StoneSpreadRadius);
		FVector Loc = StoneSpawnCenter->GetComponentLocation() + FVector(FMath::Cos(A)*Dst, FMath::Sin(A)*Dst, 0);
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		FString N = MarketSubsystem->GenerateDisplayName(D.Origin);
		float Sc = FMath::FRandRange(SC->StoneScaleRange.X, SC->StoneScaleRange.Y);
		if (AClcStone* S = GetWorld()->SpawnActor<AClcStone>(AClcStone::StaticClass(),Loc,FRotator::ZeroRotator,P)) { S->Initialize(D,M,Sc,N); SpawnedStones.Add(S); }
	}
}
FTransform AClcStoneStall::GetBallSpawnLocation() const { return BallSpawnPoint->GetComponentTransform(); }
float AClcStoneStall::GetTotalTheoreticalValue() const { float T=0; for (auto* S : SpawnedStones) if (S) T+=S->GetStoneData().Internal.TheoreticalValue; return T; }