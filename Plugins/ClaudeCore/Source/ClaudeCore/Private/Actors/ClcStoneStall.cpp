// Copyright ClaudeCore. All Rights Reserved.
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
AClcStoneStall::AClcStoneStall() {
	PrimaryActorTick.bCanEverTick=false;
	StallMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StallMesh"));
	RootComponent=StallMesh;
	StallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DM(TEXT("/Engine/BasicShapes/Cube"));
	if (DM.Succeeded()) { StallMesh->SetStaticMesh(DM.Object); StallMesh->SetRelativeScale3D(FVector(1,1,0.2f)); }
	BallSpawnPoint=CreateDefaultSubobject<USceneComponent>(TEXT("BallSpawnPoint"));
	BallSpawnPoint->SetupAttachment(StallMesh); BallSpawnPoint->SetRelativeLocation(FVector(0,0,200));
	StoneSpawnCenter=CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnCenter"));
	StoneSpawnCenter->SetupAttachment(StallMesh); StoneSpawnCenter->SetRelativeLocation(FVector(0,0,10));
	PreviewGrid=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewGrid"));
	PreviewGrid->SetupAttachment(StallMesh); PreviewGrid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewGrid->SetCastShadow(false); PreviewGrid->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PM(TEXT("/Engine/BasicShapes/Cube"));
	if (PM.Succeeded()) PreviewGrid->SetStaticMesh(PM.Object);
}
void AClcStoneStall::BeginPlay() { Super::BeginPlay(); PreviewGrid->SetVisibility(false); MarketSubsystem=GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>(); if (MarketSubsystem) MarketSubsystem->RegisterStall(this); SpawnStones(); }
void AClcStoneStall::OnConstruction(const FTransform& T) { Super::OnConstruction(T);
#if WITH_EDITOR
	RefreshEditorPreview();
#endif
}
bool AClcStoneStall::CalcGridLayout(int32 N, int32& C, int32& R) const { if (N<=0) return false; C=FMath::CeilToInt(FMath::Sqrt((float)N)); R=FMath::CeilToInt((float)N/(float)C); return true; }
void AClcStoneStall::RefreshEditorPreview() {
#if WITH_EDITOR
	UClcStallConfig* Cfg=LoadObject<UClcStallConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StallConfig"));
	if (!Cfg) return;
	int32 Cols,Rows; if (!CalcGridLayout(Cfg->StonesPerStall,Cols,Rows)) return;
	float CS=Cfg->UnitCellSize, M=CS*Cfg->MarginCells;
	StallMesh->SetRelativeScale3D(FVector((CS*Cols+M*2)/100.f,(CS*Rows+M*2)/100.f,Cfg->StallThicknessScale));
	StoneSpawnCenter->SetRelativeLocation(FVector(0,0,50.f*Cfg->StallThicknessScale));
	BuildGridPreview(Cols,Rows,CS);
#endif
}
void AClcStoneStall::BuildGridPreview(int32 Cols,int32 Rows,float CellSize) {
#if WITH_EDITOR
	if (!PreviewGrid) return; PreviewGrid->ClearInstances(); PreviewGrid->SetVisibility(true);
	float CVS=CellSize*0.9f; FVector S(CVS/100.f,CVS/100.f,0.05f); float TTZ=50.f*0.2f*2.f;
	for (int32 R=0;R<Rows;R++) for (int32 C=0;C<Cols;C++) {
		float OX=(C-(Cols-1)*0.5f)*CellSize, OY=(R-(Rows-1)*0.5f)*CellSize;
		FTransform T; T.SetLocation(FVector(OX,OY,TTZ+5)); T.SetScale3D(S); T.SetRotation(FQuat::Identity);
		PreviewGrid->AddInstanceWorldSpace(T);
	}
#endif
}
void AClcStoneStall::SpawnStones() {
	for (auto* S : SpawnedStones) if(S) S->Destroy(); SpawnedStones.Empty();
	if (!MarketSubsystem) return;
	auto* SC=MarketSubsystem->GetStallConfig(); auto* MC=MarketSubsystem->GetMeshConfig();
	if (!SC||!MC) return;
	int32 Cols,Rows; if (!CalcGridLayout(SC->StonesPerStall,Cols,Rows)) return;
	float CS=SC->UnitCellSize; FVector Ct=StoneSpawnCenter->GetComponentLocation(); float Jt=CS*SC->CellJitterRatio;
	for (int32 i=0;i<SC->StonesPerStall;i++) {
		int32 C=i%Cols,R=i/Cols;
		float OX=(C-(Cols-1)*0.5f)*CS, OY=(R-(Rows-1)*0.5f)*CS;
		bool Ok; auto D=MarketSubsystem->GenerateStoneInternal(Ok); if(!Ok) continue;
		UStaticMesh* M=MC->GetRandomMesh(); if(!M) continue;
		auto B=M->GetBounds(); float Radius=B.SphereRadius;
		float Scl=FMath::FRandRange(SC->StoneScaleRange.X,SC->StoneScaleRange.Y);
		if(Radius>0) Scl=FMath::Min(Scl,CS/(Radius*2));
		float Btm=(B.BoxExtent.Z-B.Origin.Z)*Scl;
		FVector Loc=Ct+FVector(OX+FMath::FRandRange(-Jt,Jt),OY+FMath::FRandRange(-Jt,Jt),Btm);
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		FString N=MarketSubsystem->GenerateDisplayName(D.Origin);
		if(auto* S=GetWorld()->SpawnActor<AClcStone>(AClcStone::StaticClass(),Loc,FRotator(0,FMath::FRandRange(0,360),0),P)) { S->Initialize(D,M,Scl,N); SpawnedStones.Add(S); }
	}
}
FTransform AClcStoneStall::GetBallSpawnLocation() const { return BallSpawnPoint->GetComponentTransform(); }
float AClcStoneStall::GetTotalTheoreticalValue() const { float T=0; for(auto* S:SpawnedStones) if(S) T+=S->GetStoneData().Internal.TheoreticalValue; return T; }