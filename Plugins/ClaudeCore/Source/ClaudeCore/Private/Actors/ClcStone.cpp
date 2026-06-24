// Copyright ClaudeCore. All Rights Reserved.
#include "Actors/ClcStone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "UI/ClcStoneInfoWidget.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Kismet/GameplayStatics.h"
AClcStone::AClcStone() {
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=0.2f;
	StoneMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
	RootComponent=StoneMesh;
	StoneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StoneMesh->SetGenerateOverlapEvents(false);
	InteractionIndicator=CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
	InteractionIndicator->InteractionRadius=400.0f;
}
void AClcStone::BeginPlay() { Super::BeginPlay(); InfoCardClass = LoadClass<UClcStoneInfoWidget>(nullptr, TEXT("/Game/JadeBetting/UI/WBP_StoneInfo.WBP_StoneInfo_C")); }
void AClcStone::Initialize(const FClcStoneInternalData& D, UStaticMesh* M, float S, const FString& N) {
	RuntimeData.Internal=D; RuntimeData.DisplayName=N; RuntimeData.BackpackIndex=-1;
	RuntimeData.AccumulatedOpenedArea=0; RuntimeData.OpenedGreenArea=0; RuntimeData.OpenedBlackArea=0; RuntimeData.LargestExposedGreenPatch=0;
	if (M) StoneMesh->SetStaticMesh(M);
	SetActorScale3D(FVector(S));
	RecalculateSurfaceArea();
}
void AClcStone::RecalculateSurfaceArea() {
	if (!StoneMesh||!StoneMesh->GetStaticMesh()) { RuntimeData.Internal.SurfaceArea=1000; return; }
	float R = StoneMesh->GetStaticMesh()->GetBounds().SphereRadius * GetActorScale3D().GetMax();
	RuntimeData.Internal.SurfaceArea = 4.0f*PI*R*R*0.8f;
}
void AClcStone::Tick(float DT) {
	Super::Tick(DT);
	if ((RangeCheckTimer-=DT)>0) return; RangeCheckTimer=0.3f;
	bCameraAiming = (InteractionIndicator->GetInteractionState() == 2);
	if (bCameraAiming&&!bInfoCardVisible) ShowInfoCard();
	else if (!bCameraAiming&&bInfoCardVisible) HideInfoCard();
}
FText AClcStone::GetInteractionPrompt() const { return FText::FromString(FString::Printf(TEXT("购买 %s - %d 金币"), *RuntimeData.DisplayName, RuntimeData.Internal.PurchasePrice)); }
bool AClcStone::OnInteract(AActor* I) {
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); if (!PC) return false;
	UClcBackpackSubsystem* B = PC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>(); if (!B) return false;
	if (!B->SpendGold(RuntimeData.Internal.PurchasePrice)) return false;
	B->AddStone(RuntimeData);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2, FColor::Green, FString::Printf(TEXT("购买成功！%s 已加入背包"), *RuntimeData.DisplayName));
	HideInfoCard(); RemoveFromStall(); return true;
}
bool AClcStone::PurchaseStone(AActor* B) { return OnInteract(B); }
void AClcStone::ShowInfoCard() {
	if (bInfoCardVisible||!InfoCardClass) return;
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0); if (!PC) return;
	InfoCardWidget = CreateWidget<UClcStoneInfoWidget>(PC, InfoCardClass);
	if (InfoCardWidget) { InfoCardWidget->AddToViewport(50); InfoCardWidget->ShowInfo(RuntimeData); bInfoCardVisible=true; }
}
void AClcStone::HideInfoCard() { if (InfoCardWidget) { InfoCardWidget->HideInfo(); InfoCardWidget->RemoveFromParent(); InfoCardWidget=nullptr; } bInfoCardVisible=false; }
void AClcStone::RemoveFromStall() { HideInfoCard(); Destroy(); }