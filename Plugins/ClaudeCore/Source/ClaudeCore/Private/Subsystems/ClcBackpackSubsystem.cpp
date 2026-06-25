// Copyright ClaudeCore. All Rights Reserved.
#include "Subsystems/ClcBackpackSubsystem.h"
#include "UI/ClcBackpackWidget.h"
#include "Data/ClcStoneConfig.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
void UClcBackpackSubsystem::Initialize(FSubsystemCollectionBase& C) {
	Super::Initialize(C);
	BackpackWidgetClass=LoadClass<UClcBackpackWidget>(nullptr,TEXT("/Game/JadeBetting/UI/WBP_Backpack.WBP_Backpack_C"));
	if (UClcStoneConfig* Cfg=LoadObject<UClcStoneConfig>(nullptr,TEXT("/Game/JadeBetting/Data/DA_StoneConfig")))
		Gold=Cfg->InitialGold;
}
void UClcBackpackSubsystem::Deinitialize() { if(BackpackWidget&&BackpackWidget->IsInViewport()) BackpackWidget->RemoveFromParent(); BackpackWidget=nullptr; Super::Deinitialize(); }
void UClcBackpackSubsystem::ToggleBackpack() {
	APlayerController* PC=GetLocalPlayer()?GetLocalPlayer()->GetPlayerController(GetWorld()):nullptr; if(!PC) return;
	if(!bIsOpen) {
		if(!BackpackWidget) { if(!BackpackWidgetClass) return; BackpackWidget=CreateWidget<UClcBackpackWidget>(PC,BackpackWidgetClass); if(!BackpackWidget) return; }
		if(BackpackWidget) { BackpackWidget->AddToViewport(100); BackpackWidget->RefreshDisplay(Stones,Gold); bIsOpen=true; PC->bShowMouseCursor=true; UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC,BackpackWidget); }
	} else { if(BackpackWidget) BackpackWidget->RemoveFromParent(); bIsOpen=false; PC->bShowMouseCursor=false; UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC); }
}
TArray<FClcStoneRuntimeData> UClcBackpackSubsystem::GetStones() const { return Stones; }
int32 UClcBackpackSubsystem::AddStone(const FClcStoneRuntimeData& D) {
	if(Stones.Num()>=MAX_STONE_SLOTS) { UE_LOG(LogTemp,Error,TEXT("[ClcBackpack] MAX_STONE_SLOTS exceeded!")); return -1; }
	int32 I=Stones.Add(D); Stones[I].BackpackIndex=I;
	if(BackpackWidget&&bIsOpen) BackpackWidget->RefreshDisplay(Stones,Gold);
	return I;
}
bool UClcBackpackSubsystem::RemoveStone(int32 I) { if(!Stones.IsValidIndex(I)) return false; Stones.RemoveAt(I); for(int32 j=I;j<Stones.Num();j++) Stones[j].BackpackIndex=j; if(BackpackWidget&&bIsOpen) BackpackWidget->RefreshDisplay(Stones,Gold); return true; }
int32 UClcBackpackSubsystem::GetGold() const { return Gold; }
void UClcBackpackSubsystem::AddGold(int32 A) { Gold+=A; TotalEarned+=FMath::Max(0,A); if(BackpackWidget&&bIsOpen) BackpackWidget->RefreshDisplay(Stones,Gold); }
bool UClcBackpackSubsystem::SpendGold(int32 A) { if(Gold<A){ ShowNotification(FString::Printf(TEXT("金币不足！需要 %d，当前 %d"),A,Gold)); return false; } Gold-=A; if(BackpackWidget&&bIsOpen) BackpackWidget->RefreshDisplay(Stones,Gold); return true; }
FClcStoneRuntimeData* UClcBackpackSubsystem::GetStoneMutable(int32 I) { return Stones.IsValidIndex(I)?&Stones[I]:nullptr; }
void UClcBackpackSubsystem::UpdateStoneData(int32 I,const FClcStoneRuntimeData& D) { if(Stones.IsValidIndex(I)){Stones[I]=D;Stones[I].BackpackIndex=I;} }
void UClcBackpackSubsystem::ShowNotification(const FString& M) { if(GEngine) GEngine->AddOnScreenDebugMessage(-1,2.5f,FColor::Yellow,M); }
void UClcBackpackSubsystem::GMAddGold(int32 A) { Gold+=A; if(GEngine) GEngine->AddOnScreenDebugMessage(-1,3,FColor::Green,FString::Printf(TEXT("[GM] Added %d gold. Total: %d"),A,Gold)); }