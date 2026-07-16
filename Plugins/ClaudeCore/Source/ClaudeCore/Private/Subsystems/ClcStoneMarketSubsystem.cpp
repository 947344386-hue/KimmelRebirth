// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "ClcLog.h"
#include "Data/ClcStoneConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcShellTextureConfig.h"
#include "ClcDeveloperSettings.h"
#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Engine/AssetManager.h"

namespace
{
	// DA_StoneConfig 未配置 GradeRollWeights 时的内置默认——豆40/糯30/冰20/玻10
	// 避免 RollGrade 在空 Map 时静默 fallback 全豆种
	const TMap<EClcJadeGrade, float>& GetDefaultGradeRollWeights()
	{
		static const TMap<EClcJadeGrade, float> M = []{
			TMap<EClcJadeGrade, float> T;
			T.Add(EClcJadeGrade::Bean, 40.0f);
			T.Add(EClcJadeGrade::Glutinous, 30.0f);
			T.Add(EClcJadeGrade::Ice, 20.0f);
			T.Add(EClcJadeGrade::Glass, 10.0f);
			return T;
		}();
		return M;
	}

	// DA_StoneConfig 未配置 GradeValueMultiplier 时的内置默认——豆1/糯2/冰4/玻8
	// 避免定价函数在空 Map 时所有种水都用 1.0 系数，种水不分档
	const TMap<EClcJadeGrade, float>& GetDefaultGradeValueMultipliers()
	{
		static const TMap<EClcJadeGrade, float> M = []{
			TMap<EClcJadeGrade, float> T;
			T.Add(EClcJadeGrade::Bean, 1.0f);
			T.Add(EClcJadeGrade::Glutinous, 2.0f);
			T.Add(EClcJadeGrade::Ice, 4.0f);
			T.Add(EClcJadeGrade::Glass, 8.0f);
			return T;
		}();
		return M;
	}
}

void UClcStoneMarketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 加载配置DataAsset——路径从 DeveloperSettings 读，挪资产只改 Project Settings
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (!DS) return;

	StoneConfig = LoadObject<UClcStoneConfig>(nullptr, *DS->StoneConfigPath);
	MeshConfig = LoadObject<UClcStoneMeshConfig>(nullptr, *DS->StoneMeshConfigPath);
	StallConfig = LoadObject<UClcStallConfig>(nullptr, *DS->StallConfigPath);
	ShellTextureConfig = LoadObject<UClcShellTextureConfig>(nullptr, *DS->ShellTextureConfigPath);

	if (!StoneConfig)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMarket] Failed to load StoneConfig! Path: %s (check Project Settings → ClaudeCore)"), *DS->StoneConfigPath);
	}
	if (!MeshConfig)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMarket] Failed to load StoneMeshConfig! Path: %s (check Project Settings → ClaudeCore)"), *DS->StoneMeshConfigPath);
	}
	if (!StallConfig)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMarket] Failed to load StallConfig! Path: %s (check Project Settings → ClaudeCore)"), *DS->StallConfigPath);
	}
	if (!ShellTextureConfig)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMarket] ShellTextureConfig not found. Path: %s (check Project Settings → ClaudeCore)"), *DS->ShellTextureConfigPath);
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

FClcStoneInternalData UClcStoneMarketSubsystem::GenerateStoneInternal(bool& bOutSuccess, float DeceptionLevel)
{
	bOutSuccess = false;
	FClcStoneInternalData Data;

	if (!StoneConfig || StoneConfig->Origins.Num() == 0)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcMarket] Cannot generate stone: missing config or origins."));
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

	// 2b. 商人吹卖黑话：按真实种水+欺骗倾向 roll 声称档，从黑话池取 2 条拼成 ClaimedPitch
	EClcJadeGrade ClaimedGrade = Data.Grade;
	if (ClaimedGrade != EClcJadeGrade::Glass)
	{
		if (Random.GetFraction() < DeceptionLevel)
		{
			ClaimedGrade = static_cast<EClcJadeGrade>(FMath::Min<int32>(static_cast<int32>(ClaimedGrade) + 1, static_cast<int32>(EClcJadeGrade::Glass)));
			if (Random.GetFraction() < 0.2f)
			{
				ClaimedGrade = static_cast<EClcJadeGrade>(FMath::Min<int32>(static_cast<int32>(ClaimedGrade) + 1, static_cast<int32>(EClcJadeGrade::Glass)));
			}
		}
	}
	const TArray<FText>* Pool = nullptr;
	for (const FClcPitchPool& P : StoneConfig->JadePitchPool)
	{
		if (P.Grade == ClaimedGrade) { Pool = &P.Phrases; break; }
	}
	if (Pool)
	{
		const int32 N = Pool->Num();
		if (N > 0)
		{
			const int32 IdxA = Random.RandRange(0, N - 1);
			const int32 IdxB = (N >= 2) ? (IdxA + 1 + Random.RandRange(0, N - 2)) % N : IdxA;
			Data.ClaimedPitch = (*Pool)[IdxA].ToString() + (*Pool)[IdxB].ToString();
		}
	}
	if (Data.ClaimedPitch.IsEmpty())
	{
		if (const UEnum* Enum = StaticEnum<EClcJadeGrade>())
		{
			Data.ClaimedPitch = Enum->GetDisplayNameTextByValue(static_cast<int32>(ClaimedGrade)).ToString();
		}
	}

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
	const bool bUsingDefault = !StoneConfig || StoneConfig->GradeRollWeights.Num() == 0;
	const TMap<EClcJadeGrade, float>& Weights = bUsingDefault
		? GetDefaultGradeRollWeights()
		: StoneConfig->GradeRollWeights;
	if (bUsingDefault)
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMarket] GradeRollWeights 未配置，使用内置默认(豆40/糯30/冰20/玻10)。请在 DA_StoneConfig 填表以自定义。"));
	}

	// 构建加权表（基础权重 + 产地加成）
	TArray<TPair<EClcJadeGrade, float>> WeightedTable;

	for (const auto& Pair : Weights)
	{
		float Weight = Pair.Value;
		// 产地软关联：遍历 OriginGradeBonuses 数组查找匹配的产地和对应的种水加成
		if (StoneConfig)
		{
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

	OutGreen = Random.FRandRange(StoneConfig->GreenRatioRange.X, StoneConfig->GreenRatioRange.Y);
	// 黑面积上限不超过 (1 - 绿面积) 且不超过配置上限
	const float MaxBlack = FMath::Min(1.0f - OutGreen, StoneConfig->BlackRatioRange.Y);
	OutBlack = Random.FRandRange(StoneConfig->BlackRatioRange.X, MaxBlack);
	OutLargestPatch = Random.FRandRange(StoneConfig->LargestPatchRatioRange.X, StoneConfig->LargestPatchRatioRange.Y);
}

FString UClcStoneMarketSubsystem::GenerateDisplayName(const FClcStoneInternalData& StoneData) const
{
	// 序号按产地计数，避免同名
	int32& Counter = const_cast<TMap<FString, int32>&>(DisplayNameCounters).FindOrAdd(StoneData.Origin, 0);
	Counter++;

	const FName Shell = UClcShellTextureConfig::GetShellName(StoneData.ShellTypeIndex);

	// <产地> <皮壳> <重量>公斤 #<N> · <黑话>
	FString Base = FString::Printf(TEXT("%s %s %d公斤 #%d"),
		*StoneData.Origin, *Shell.ToString(), StoneData.WeightKg, Counter);

	if (!StoneData.ClaimedPitch.IsEmpty())
	{
		Base.Append(TEXT(" · ")).Append(StoneData.ClaimedPitch);
	}
	return Base;
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
	const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
		? StoneConfig->GradeValueMultiplier
		: GetDefaultGradeValueMultipliers();
	const float* GradeMult = ValueMults.Find(Data.Grade);
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
	if (S_total <= 0.0f) return 0;
	const float S_opened = StoneData.AccumulatedOpenedArea;
	const float S_unopened = S_total - S_opened;

	// 边界A：未开窗 → 保底价 = 理论全开价值 × 折扣系数
	// （杂裂多的石头 TheoreticalValue 低，保底自然低，避免杂裂多的石头保底和纯皮壳一样）
	if (S_opened <= 0.0f)
	{
		return FMath::RoundToInt(I.TheoreticalValue * StoneConfig->UnopenedFloorDiscountFactor);
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
	const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
		? StoneConfig->GradeValueMultiplier
		: GetDefaultGradeValueMultipliers();
	const float* GradeMult = ValueMults.Find(I.Grade);
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

FClcStoneTooltipInfo UClcStoneMarketSubsystem::BuildTooltipInfo(const FClcStoneRuntimeData& StoneData) const
{
	FClcStoneTooltipInfo Info;

	// 直接拷贝的基本字段
	Info.DisplayName = StoneData.DisplayName;
	Info.Origin = StoneData.Internal.Origin;
	Info.PurchasePrice = StoneData.Internal.PurchasePrice;

	// 当前回收价（随开窗进度实时算）
	Info.CurrentValue = CalculateSalePrice(StoneData);

	// 开到玉判定：已暴露绿色面积 > 0
	Info.bOpenedToJade = (StoneData.OpenedGreenArea > 0.0f);

	if (Info.bOpenedToJade)
	{
		// 已开到玉 → 显示种水档位（豆种/糯种/冰种/玻种）
		if (const UEnum* Enum = StaticEnum<EClcJadeGrade>())
		{
			Info.GradeText = Enum->GetDisplayNameTextByValue(static_cast<int32>(StoneData.Internal.Grade)).ToString();
		}
	}
	else
	{
		// 未开到玉 → 显示皮壳名
		Info.ShellName = UClcShellTextureConfig::GetShellName(StoneData.Internal.ShellTypeIndex).ToString();
	}

	return Info;
}
