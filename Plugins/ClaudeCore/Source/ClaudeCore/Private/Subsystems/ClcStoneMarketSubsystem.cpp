// Copyright ClaudeCore. All Rights Reserved.
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStoneConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Data/ClcStallConfig.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"

void UClcStoneMarketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StoneConfig = LoadObject<UClcStoneConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StoneConfig"));
	MeshConfig = LoadObject<UClcStoneMeshConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StoneMeshConfig"));
	StallConfig = LoadObject<UClcStallConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StallConfig"));
	if (!StoneConfig) UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StoneConfig!"));
	if (!MeshConfig) UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StoneMeshConfig!"));
	if (!StallConfig) UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StallConfig!"));
}
void UClcStoneMarketSubsystem::Deinitialize() { RegisteredStalls.Empty(); Super::Deinitialize(); }
void UClcStoneMarketSubsystem::RefreshMarket() { DisplayNameCounters.Reset(); for (auto& S : RegisteredStalls) if (AClcStoneStall* Stall = S.Get()) Stall->SpawnStones(); }
void UClcStoneMarketSubsystem::RegisterStall(AClcStoneStall* Stall) { if (Stall) RegisteredStalls.AddUnique(Stall); }
void UClcStoneMarketSubsystem::UnregisterStall(AClcStoneStall* Stall) { RegisteredStalls.Remove(Stall); }
FClcStoneInternalData UClcStoneMarketSubsystem::GenerateStoneInternal(bool& bOutSuccess)
{
	bOutSuccess = false; FClcStoneInternalData Data;
	if (!StoneConfig || StoneConfig->Origins.Num() == 0) return Data;
	const int32 Seed = FMath::Rand(); FRandomStream R(Seed);
	Data.Origin = StoneConfig->Origins[R.RandRange(0, StoneConfig->Origins.Num()-1)];
	Data.Seed = Seed; Data.Grade = RollGrade(R, Data.Origin);
	RollRatios(R, Data.GreenRatio, Data.BlackRatio, Data.LargestGreenPatchRatio);
	Data.SurfaceArea = 1000.0f;
	Data.TheoreticalValue = CalculateTheoreticalValue(Data);
	Data.PurchasePrice = CalculatePurchasePrice(Data);
	bOutSuccess = true; return Data;
}
EClcJadeGrade UClcStoneMarketSubsystem::RollGrade(FRandomStream& R, const FString& Origin) const
{
	if (!StoneConfig || StoneConfig->GradeRollWeights.Num() == 0) return EClcJadeGrade::Bean;
	TArray<TPair<EClcJadeGrade,float>> T;
	for (auto& P : StoneConfig->GradeRollWeights) {
		float W = P.Value;
		for (auto& B : StoneConfig->OriginGradeBonuses) {
			if (B.Origin == Origin) { if (const float* b = B.GradeBonuses.Find(P.Key)) W += *b; break; }
		}
		T.Add(TPair<EClcJadeGrade,float>(P.Key, FMath::Max(0.0f,W)));
	}
	float TW = 0; for (auto& P : T) TW += P.Value;
	float Roll = R.FRand() * TW, Acc = 0;
	for (auto& P : T) { Acc += P.Value; if (Roll <= Acc) return P.Key; }
	return T.Last().Key;
}
void UClcStoneMarketSubsystem::RollRatios(FRandomStream& R, float& G, float& B, float& L) const
{
	if (!StoneConfig) { G=0.3f;B=0.1f;L=0.6f;return; }
	G = FMath::FRandRange(StoneConfig->GreenRatioRange.X, StoneConfig->GreenRatioRange.Y);
	B = FMath::FRandRange(StoneConfig->BlackRatioRange.X, FMath::Min(1.0f - G, StoneConfig->BlackRatioRange.Y));
	L = FMath::FRandRange(StoneConfig->LargestPatchRatioRange.X, StoneConfig->LargestPatchRatioRange.Y);
}
FString UClcStoneMarketSubsystem::GenerateDisplayName(const FString& Origin) const
{
	int32& C = const_cast<TMap<FString,int32>&>(DisplayNameCounters).FindOrAdd(Origin, 0);
	return FString::Printf(TEXT("%s #%d"), *Origin, ++C);
}
float UClcStoneMarketSubsystem::CalculateTheoreticalValue(const FClcStoneInternalData& D) const
{
	if (!StoneConfig) return 0;
	float SG = D.SurfaceArea * D.GreenRatio, SL = SG * D.LargestGreenPatchRatio;
	float VE = (SL > StoneConfig->ContinuityAreaThreshold) ? ((SG - SL) + SL * StoneConfig->ContinuityBonusFactor) * StoneConfig->PricePerUnitArea : SG * StoneConfig->PricePerUnitArea;
	const float* GM = StoneConfig->GradeValueMultiplier.Find(D.Grade);
	return FMath::Max(0.0f, VE * (GM?*GM:1.0f) - D.SurfaceArea * D.BlackRatio * StoneConfig->PenaltyPerUnitBlack);
}
int32 UClcStoneMarketSubsystem::CalculateSalePrice(const FClcStoneRuntimeData& SD) const
{
	if (!StoneConfig) return 0;
	const FClcStoneInternalData& I = SD.Internal;
	if (SD.AccumulatedOpenedArea <= 0) return FMath::RoundToInt(I.SurfaceArea * StoneConfig->PriceFloorPerArea);
	float VE = (SD.LargestExposedGreenPatch > StoneConfig->ContinuityAreaThreshold)
		? ((SD.OpenedGreenArea - SD.LargestExposedGreenPatch) + SD.LargestExposedGreenPatch * StoneConfig->ContinuityBonusFactor) * StoneConfig->PricePerUnitArea
		: SD.OpenedGreenArea * StoneConfig->PricePerUnitArea;
	const float* GM = StoneConfig->GradeValueMultiplier.Find(I.Grade); float CS = GM?*GM:1.0f;
	float VW = VE * CS, VP = SD.OpenedBlackArea * StoneConfig->PenaltyPerUnitBlack;
	float VG = 0; float RO = I.SurfaceArea>0 ? SD.AccumulatedOpenedArea/I.SurfaceArea : 0;
	if (RO > StoneConfig->GamblingRThreshold || SD.LargestExposedGreenPatch > StoneConfig->ContinuityAreaThreshold)
		VG = VW * ((I.SurfaceArea - SD.AccumulatedOpenedArea)/I.SurfaceArea) * StoneConfig->GamblingKCoefficient;
	return FMath::RoundToInt(FMath::Max(0.0f, VW - VP + VG));
}
int32 UClcStoneMarketSubsystem::CalculatePurchasePrice(const FClcStoneInternalData& D) const
{
	if (!StoneConfig) return 0;
	return FMath::RoundToInt(D.SurfaceArea * StoneConfig->BasePricePerArea + D.TheoreticalValue * StoneConfig->HiddenPremiumFactor);
}