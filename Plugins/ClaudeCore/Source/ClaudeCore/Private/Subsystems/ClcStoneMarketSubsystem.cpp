// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStoneConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcShellTextureConfig.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Engine/AssetManager.h"

void UClcStoneMarketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 加载配置DataAsset——路径固定，设计师在编辑器里修改
	StoneConfig = LoadObject<UClcStoneConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StoneConfig"));
	MeshConfig = LoadObject<UClcStoneMeshConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StoneMeshConfig"));
	StallConfig = LoadObject<UClcStallConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StallConfig"));
	ShellTextureConfig = LoadObject<UClcShellTextureConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig"));

	if (!StoneConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StoneConfig! Create it at /Game/JadeBetting/Data/"));
	}
	if (!MeshConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StoneMeshConfig! Create it at /Game/JadeBetting/Data/"));
	}
	if (!StallConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Failed to load DA_StallConfig! Create it at /Game/JadeBetting/Data/"));
	}
	if (!ShellTextureConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClcMarket] DA_ShellTextureConfig not found at /Game/JadeBetting/Data/. Stones will use fallback shell (index 0)."));
	}
}

void UClcStoneMarketSubsystem::Deinitialize()
{
	RegisteredStalls.Empty();
	Super::Deinitialize();
}

void UClcStoneMarketSubsystem::RefreshMarket()
{
	DisplayNameCounters.Reset();

	for (const auto& StallPtr : RegisteredStalls)
	{
		if (AClcStoneStall* Stall = StallPtr.Get())
		{
			Stall->SpawnStones();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ClcMarket] Market refreshed. %d stalls."), RegisteredStalls.Num());
}

void UClcStoneMarketSubsystem::RegisterStall(AClcStoneStall* Stall)
{
	if (Stall)
	{
		RegisteredStalls.AddUnique(Stall);
	}
}

void UClcStoneMarketSubsystem::UnregisterStall(AClcStoneStall* Stall)
{
	RegisteredStalls.Remove(Stall);
}

// ============================================================
// 石头生成
// ============================================================

FClcStoneInternalData UClcStoneMarketSubsystem::GenerateStoneInternal(bool& bOutSuccess)
{
	bOutSuccess = false;
	FClcStoneInternalData Data;

	if (!StoneConfig || StoneConfig->Origins.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[ClcMarket] Cannot generate stone: missing config or origins."));
		return Data;
	}

	const int32 Seed = FMath::Rand();
	FRandomStream Random(Seed);

	// 1. 产地
	const int32 OriginIndex = Random.RandRange(0, StoneConfig->Origins.Num() - 1);
	Data.Origin = StoneConfig->Origins[OriginIndex];
	Data.Seed = Seed;

	// 1b. 皮壳类型——独立随机，和 Mesh / 分布无关
	Data.ShellTypeIndex = ShellTextureConfig ? ShellTextureConfig->GetRandomShellIndex() : 0;

	// 2. 种水（带软关联）
	Data.Grade = RollGrade(Random, Data.Origin);

	// 3. 绿/黑/大块连续比例
	RollRatios(Random, Data.GreenRatio, Data.BlackRatio, Data.LargestGreenPatchRatio);

	// 4. 表面积（由Actor创建时实际计算，这里给个估算值稍后覆盖）
	Data.SurfaceArea = 1000.0f; // 占位，创建Mesh后重新计算

	// 5. Phase 2：生成 UV 空间材质分布图（绿玉/杂裂），实测最大连续绿块比例覆盖预估
	{
		float ActualLargestPatch = 0.0f;
		Data.DistributionMap = FClcStoneDistributionMap::Generate(Seed, Data.GreenRatio, Data.BlackRatio, ActualLargestPatch);
		Data.LargestGreenPatchRatio = ActualLargestPatch;
	}

	// 6. 理论价值和购买标价
	Data.TheoreticalValue = CalculateTheoreticalValue(Data);
	Data.PurchasePrice = CalculatePurchasePrice(Data);

	bOutSuccess = true;
	return Data;
}

EClcJadeGrade UClcStoneMarketSubsystem::RollGrade(FRandomStream& Random, const FString& Origin) const
{
	if (!StoneConfig || StoneConfig->GradeRollWeights.Num() == 0)
	{
		return EClcJadeGrade::Bean;
	}

	// 构建加权表（基础权重 + 产地加成）
	TArray<TPair<EClcJadeGrade, float>> WeightedTable;

	for (const auto& Pair : StoneConfig->GradeRollWeights)
	{
		float Weight = Pair.Value;
		// 产地软关联：遍历 OriginGradeBonuses 数组查找匹配的产地和对应的种水加成
		for (const auto& BonusEntry : StoneConfig->OriginGradeBonuses)
		{
			if (BonusEntry.Origin == Origin)
			{
				if (const float* Bonus = BonusEntry.GradeBonuses.Find(Pair.Key))
				{
					Weight += *Bonus;
				}
				break;
			}
		}
		WeightedTable.Add(TPair<EClcJadeGrade, float>(Pair.Key, FMath::Max(0.0f, Weight)));
	}

	// 加权随机
	float TotalWeight = 0.0f;
	for (const auto& Pair : WeightedTable)
	{
		TotalWeight += Pair.Value;
	}

	float Roll = Random.FRand() * TotalWeight;
	float Accum = 0.0f;
	for (const auto& Pair : WeightedTable)
	{
		Accum += Pair.Value;
		if (Roll <= Accum)
		{
			return Pair.Key;
		}
	}

	return WeightedTable.Last().Key;
}

void UClcStoneMarketSubsystem::RollRatios(FRandomStream& Random, float& OutGreen, float& OutBlack, float& OutLargestPatch) const
{
	if (!StoneConfig)
	{
		OutGreen = 0.3f; OutBlack = 0.1f; OutLargestPatch = 0.6f;
		return;
	}

	OutGreen = FMath::FRandRange(StoneConfig->GreenRatioRange.X, StoneConfig->GreenRatioRange.Y);
	// 黑面积上限不超过 (1 - 绿面积) 且不超过配置上限
	const float MaxBlack = FMath::Min(1.0f - OutGreen, StoneConfig->BlackRatioRange.Y);
	OutBlack = FMath::FRandRange(StoneConfig->BlackRatioRange.X, MaxBlack);
	OutLargestPatch = FMath::FRandRange(StoneConfig->LargestPatchRatioRange.X, StoneConfig->LargestPatchRatioRange.Y);
}

FString UClcStoneMarketSubsystem::GenerateDisplayName(const FString& Origin) const
{
	int32& Counter = const_cast<TMap<FString, int32>&>(DisplayNameCounters).FindOrAdd(Origin, 0);
	Counter++;
	return FString::Printf(TEXT("%s #%d"), *Origin, Counter);
}

// ============================================================
// 定价公式 V2
// ============================================================

float UClcStoneMarketSubsystem::CalculateTheoreticalValue(const FClcStoneInternalData& Data) const
{
	if (!StoneConfig) return 0.0f;

	// 面积
	const float S_green = Data.SurfaceArea * Data.GreenRatio;
	const float S_largest = S_green * Data.LargestGreenPatchRatio;
	const float S_threshold = StoneConfig->ContinuityAreaThreshold;
	const float C_continuity = StoneConfig->ContinuityBonusFactor;

	// 基础价值
	float V_exposed;
	if (S_largest > S_threshold)
	{
		V_exposed = ((S_green - S_largest) + S_largest * C_continuity) * StoneConfig->PricePerUnitArea;
	}
	else
	{
		V_exposed = S_green * StoneConfig->PricePerUnitArea;
	}

	// 种水
	const float* GradeMult = StoneConfig->GradeValueMultiplier.Find(Data.Grade);
	const float C_sw = GradeMult ? *GradeMult : 1.0f;
	const float V_weighted = V_exposed * C_sw;

	// 杂裂惩罚
	const float S_black = Data.SurfaceArea * Data.BlackRatio;
	const float V_penalty = S_black * StoneConfig->PenaltyPerUnitBlack;

	return FMath::Max(0.0f, V_weighted - V_penalty);
}

int32 UClcStoneMarketSubsystem::CalculateSalePrice(const FClcStoneRuntimeData& StoneData) const
{
	if (!StoneConfig) return 0;

	const FClcStoneInternalData& I = StoneData.Internal;
	const float S_total = I.SurfaceArea;
	const float S_opened = StoneData.AccumulatedOpenedArea;
	const float S_unopened = S_total - S_opened;

	// 边界A：未开窗 → 保底价
	if (S_opened <= 0.0f)
	{
		return FMath::RoundToInt(S_total * StoneConfig->PriceFloorPerArea);
	}

	// ---- 已暴露基础价值 ----
	const float S_green = StoneData.OpenedGreenArea;
	const float S_black = StoneData.OpenedBlackArea;
	const float S_largest = StoneData.LargestExposedGreenPatch;
	const float S_threshold = StoneConfig->ContinuityAreaThreshold;

	float V_exposed;
	if (S_largest > S_threshold)
	{
		V_exposed = ((S_green - S_largest) + S_largest * StoneConfig->ContinuityBonusFactor) * StoneConfig->PricePerUnitArea;
	}
	else
	{
		V_exposed = S_green * StoneConfig->PricePerUnitArea;
	}

	// 种水
	const float* GradeMult = StoneConfig->GradeValueMultiplier.Find(I.Grade);
	const float C_sw = GradeMult ? *GradeMult : 1.0f;
	const float V_weighted = V_exposed * C_sw;

	// 杂裂惩罚
	const float V_penalty = S_black * StoneConfig->PenaltyPerUnitBlack;

	// 剩余赌价（边界B）
	float V_gambling = 0.0f;
	const float R_opened = S_total > 0.0f ? (S_opened / S_total) : 0.0f;
	const bool bCondition = (R_opened > StoneConfig->GamblingRThreshold) ||
		(S_largest > StoneConfig->ContinuityAreaThreshold);

	if (bCondition)
	{
		V_gambling = V_weighted * (S_unopened / S_total) * StoneConfig->GamblingKCoefficient;
	}

	const float V_final = FMath::Max(0.0f, V_weighted - V_penalty + V_gambling);
	return FMath::RoundToInt(V_final);
}

int32 UClcStoneMarketSubsystem::CalculatePurchasePrice(const FClcStoneInternalData& Data) const
{
	if (!StoneConfig) return 0;

	// 基础标价 = 表面积 × 基础单价
	const float BasePrice = Data.SurfaceArea * StoneConfig->BasePricePerArea;

	// 隐藏溢价 = 理论价值 × 溢价系数
	const float Premium = Data.TheoreticalValue * StoneConfig->HiddenPremiumFactor;

	return FMath::RoundToInt(BasePrice + Premium);
}
