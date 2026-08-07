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
	// ---- 连续暴击（算法内部常量，已从 DA 移除，按 SA≈16000 标定）----
	static constexpr float ContinuityAreaThreshold = 800.0f;  // 单块连续玉肉超过此面积触发暴击（约 SA 5%）
	static constexpr float ContinuityBonusFactor = 2.0f;      // 暴击倍率：达标大块面积按此系数计价（2.0=翻倍）

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

	FString BuildProductName(const FString& Pitch)
	{
		if (Pitch.Contains(TEXT("新坑味")))
		{
			return TEXT("新坑料");
		}

		const bool bGlue = Pitch.Contains(TEXT("起胶感"));
		const bool bTransparent = Pitch.Contains(TEXT("透度好"));
		const bool bLongWater = Pitch.Contains(TEXT("水头长")) || Pitch.Contains(TEXT("水长"));
		const bool bMediumWater = Pitch.Contains(TEXT("水头中"));
		const bool bShortWater = Pitch.Contains(TEXT("水短"));
		const bool bFine = Pitch.Contains(TEXT("肉尚细"));
		const bool bCoarse = Pitch.Contains(TEXT("肉粗"));

		FString Prefix;
		if (Pitch.Contains(TEXT("高冰味")))
		{
			Prefix = TEXT("冰");
		}
		else if (Pitch.Contains(TEXT("老味足")) || Pitch.Contains(TEXT("种份老")) || Pitch.Contains(TEXT("老种")))
		{
			Prefix = TEXT("老");
		}
		else if (Pitch.Contains(TEXT("糯化感")) || Pitch.Contains(TEXT("种尚可")))
		{
			Prefix = TEXT("糯");
		}
		else if (Pitch.Contains(TEXT("嫩种")))
		{
			Prefix = TEXT("嫩");
		}

		if (Prefix.IsEmpty())
		{
			if (bGlue && (bLongWater || bMediumWater || bShortWater)) return TEXT("胶润料");
			if (bTransparent && (bLongWater || bMediumWater || bShortWater)) return TEXT("水透料");
			if (bFine && bMediumWater) return TEXT("润细料");
			if (bCoarse && bShortWater) return TEXT("粗水料");
		}

		FString Suffix;
		if (bGlue) Suffix = TEXT("胶");
		else if (bTransparent) Suffix = TEXT("透");
		else if (bFine) Suffix = TEXT("细");
		else if (bMediumWater) Suffix = TEXT("润");
		else if (bLongWater || bShortWater) Suffix = TEXT("水");
		else if (bCoarse) Suffix = TEXT("粗");

		if (!Prefix.IsEmpty() && !Suffix.IsEmpty())
		{
			return Prefix + Suffix + TEXT("料");
		}
		if (Prefix == TEXT("老")) return TEXT("老味料");
		if (Prefix == TEXT("冰")) return TEXT("冰味料");
		if (Prefix == TEXT("糯")) return TEXT("糯化料");
		if (Prefix == TEXT("嫩")) return TEXT("嫩种料");
		if (!Suffix.IsEmpty()) return Suffix + TEXT("润料");
		return TEXT("原石料");
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

	// 1b. 皮壳类型——走 Seed 流（确定性），和 Mesh / 分布无关；同 Seed 重建可复现
	Data.ShellTypeIndex = ShellTextureConfig ? ShellTextureConfig->GetRandomShellIndex(Random) : 0;

	// 2. 种水（独立维度：仅作单位面积玉肉的价值系数，与杂玉比例无关）
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

	// 3. 表面积（由Actor创建时实际计算，这里给个估算值稍后覆盖）
	Data.SurfaceArea = 1000.0f; // 占位，创建Mesh后重新计算

	// 4. 有机缺陷体模型：默认整块是玉，生成 2-5 个连续不规则缺陷体（蛛网/团球触手）。杂玉占比与种水独立。
	{
		// 杂玉占比：独立随机（与种水无关），5%~85% 宽随机——占比即"玉多还是杂多"
		const float TargetCoverage = Random.FRandRange(
			StoneConfig->CrackCoverageRange.X, StoneConfig->CrackCoverageRange.Y);
		// 缺陷块数量随占比上升（2→5）
		const int32 DefectCount = FMath::Clamp(2 + FMath::RoundToInt(TargetCoverage * 4.0f), 2, 5);

		FClcStoneDistributionMap::FMeasureResult Actuals;
		Data.DistributionMap = FClcStoneDistributionMap::Generate(
			Seed, DefectCount, TargetCoverage, Actuals);

		const int32 Total = FClcStoneDistributionMap::Resolution * FClcStoneDistributionMap::Resolution;
		const float InvTotal = Total > 0 ? 1.0f / static_cast<float>(Total) : 0.0f;
		// 实测权威：有机缺陷体模型下 玉 + 缺陷 = 全图
		Data.CrackRatio    = Actuals.CrackPixels * InvTotal;
		Data.GreenRatio    = Actuals.JadePixels  * InvTotal; // = 1 - 缺陷
		Data.ImpurityRatio = 0.0f;
		Data.BlackRatio    = Data.CrackRatio;                 // 兼容旧字段
		Data.LargestGreenPatchRatio = (Actuals.JadePixels > 0)
			? static_cast<float>(Actuals.LargestJadePatchPixels) / static_cast<float>(Actuals.JadePixels)
			: 0.0f;
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

FString UClcStoneMarketSubsystem::GenerateDisplayName(const FClcStoneInternalData& StoneData) const
{
	return BuildProductName(StoneData.ClaimedPitch);
}

// ============================================================
// 定价公式 V2
// ============================================================

float UClcStoneMarketSubsystem::CalculateTheoreticalValue(const FClcStoneInternalData& Data) const
{
	if (!StoneConfig) return 0.0f;

	// 玉肉（用实测占比）
	const float S_jade = Data.SurfaceArea * Data.GreenRatio;
	const float S_largest = S_jade * Data.LargestGreenPatchRatio;
	const float S_threshold = ContinuityAreaThreshold;
	const float C_continuity = ContinuityBonusFactor;

	// 基础价值（含大块连续暴击）
	float V_exposed;
	if (S_largest > S_threshold)
	{
		V_exposed = ((S_jade - S_largest) + S_largest * C_continuity) * StoneConfig->PricePerUnitArea;
	}
	else
	{
		V_exposed = S_jade * StoneConfig->PricePerUnitArea;
	}

	// 种水
	const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
		? StoneConfig->GradeValueMultiplier
		: GetDefaultGradeValueMultipliers();
	const float* GradeMult = ValueMults.Find(Data.Grade);
	const float C_sw = GradeMult ? *GradeMult : 1.0f;
	const float V_weighted = V_exposed * C_sw;

	// 杂裂统一惩罚（裂纹切割模型：缺陷=裂纹，越密越碎价值越低）
	const float V_penalty =
		Data.SurfaceArea * (Data.ImpurityRatio + Data.CrackRatio) * StoneConfig->PenaltyPerUnitCrack;

	return FMath::Max(0.0f, V_weighted - V_penalty);
}

int32 UClcStoneMarketSubsystem::CalculateSalePrice(const FClcStoneRuntimeData& StoneData) const
{
	if (!StoneConfig) return 0;

	// 解石阶段 → 3D 体积定价（独立于 2D 面积定价）
	if (StoneData.Phase == EClcStonePhase::Cut)
	{
		return CalculateCutStoneSalePrice(StoneData);
	}

	const FClcStoneInternalData& I = StoneData.Internal;
	const float S_total = I.SurfaceArea;
	if (S_total <= 0.0f) return 0;
	const float S_opened = StoneData.AccumulatedOpenedArea;
	const float S_unopened = S_total - S_opened;

	// 边界A：未擦石 → 保底价 = 理论全开价值 × 折扣系数
	// （杂裂多的石头 TheoreticalValue 低，保底自然低，避免杂裂多的石头保底和纯皮壳一样）
	if (S_opened <= 0.0f)
	{
		return FMath::RoundToInt(I.TheoreticalValue * StoneConfig->UnopenedFloorDiscountFactor);
	}

	// ---- 已暴露基础价值（玉肉） ----
	const float S_green = StoneData.OpenedGreenArea;
	const float S_largest = StoneData.LargestExposedGreenPatch;
	const float S_threshold = ContinuityAreaThreshold;

	float V_exposed;
	if (S_largest > S_threshold)
	{
		V_exposed = ((S_green - S_largest) + S_largest * ContinuityBonusFactor) * StoneConfig->PricePerUnitArea;
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

	// 杂裂统一惩罚（裂纹切割模型，与理论价同源，保证全开收敛）
	const float V_penalty =
		(StoneData.OpenedImpurityArea + StoneData.OpenedCrackArea) * StoneConfig->PenaltyPerUnitCrack;

	// 净外推赌价：把已开区域的净价值密度对称外推到未开区域，从第一窗就激活。
	// 富窗净价值高→正赌价（吹高）；穷窗净价值低/负→负赌价（快速走低）。
	// 全开时 S_unopened=0 → 赌价=0 → 回收价=净价值=理论价值（收敛不变）。
	const float V_net = V_weighted - V_penalty;
	const float V_gambling = V_net * (S_unopened / S_total) * StoneConfig->GamblingKCoefficient;
	const float V_final = FMath::Max(0.0f, V_net + V_gambling);
	return FMath::RoundToInt(V_final);
}

int32 UClcStoneMarketSubsystem::CalculateHagglePrice(int32 BasePrice, float Ratio, bool bSuccess, bool bSymmetricFailure) const
{
	// 成功上浮 Ratio；失败时 bSymmetricFailure=true 下折同等比例（对称赔率），false 则按原价（仅失去加价机会，不倒扣）。
	const float Mult = bSuccess ? (1.0f + Ratio) : (bSymmetricFailure ? (1.0f - Ratio) : 1.0f);
	return FMath::Max(0, FMath::RoundToInt(static_cast<float>(BasePrice) * Mult));
}

// ============================================================
// 解石（3D 体积）定价
// ============================================================

int32 UClcStoneMarketSubsystem::CalculateCutPieceValue(int32 CutAwayTotal, int32 CutAwayJade,
	int32 CutAwayCrack, float VoxelVolume, EClcJadeGrade Grade) const
{
	if (!StoneConfig || CutAwayTotal <= 0) return 0;

	const float V_total = static_cast<float>(CutAwayTotal) * VoxelVolume;
	if (V_total < StoneConfig->MinVolumeForValue)
	{
		return 0;
	}

	const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
		? StoneConfig->GradeValueMultiplier
		: GetDefaultGradeValueMultipliers();
	const float* GradeMult = ValueMults.Find(Grade);
	const float C_sw = GradeMult ? *GradeMult : 1.0f;

	const float V_jade  = static_cast<float>(CutAwayJade)  * VoxelVolume;
	const float V_crack = static_cast<float>(CutAwayCrack) * VoxelVolume;

	const float V_value = V_jade * StoneConfig->PricePerUnitVolume * C_sw
		- V_crack * StoneConfig->PenaltyPerUnitCrackVolume;

	return FMath::Max(0, FMath::RoundToInt(V_value));
}

int32 UClcStoneMarketSubsystem::CalculateCutStoneSalePrice(const FClcStoneRuntimeData& StoneData) const
{
	if (!StoneConfig) return 0;

	const float V_total = StoneData.ExposedCutVolume + StoneData.RemainingVolume;
	if (V_total <= 0.0f) return 0;

	// 边界A：尚未下刀 → 保底价 = 理论全开价值 × 折扣系数
	if (StoneData.ExposedCutVolume <= 0.0f)
	{
		return FMath::RoundToInt(StoneData.Internal.TheoreticalValue * StoneConfig->UnopenedFloorDiscountFactor);
	}

	const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
		? StoneConfig->GradeValueMultiplier
		: GetDefaultGradeValueMultipliers();
	const float* GradeMult = ValueMults.Find(StoneData.Internal.Grade);
	const float C_sw = GradeMult ? *GradeMult : 1.0f;

	// 已露截面价值
	const float V_jade_value  = StoneData.ExposedJadeVolume  * StoneConfig->PricePerUnitVolume * C_sw;
	const float V_crack_pen   = StoneData.ExposedCrackVolume * StoneConfig->PenaltyPerUnitCrackVolume;
	const float V_net = V_jade_value - V_crack_pen;

	// 净外推：把已露截面净价值密度对称外推到未切体积
	const float V_unexposed = StoneData.RemainingVolume;
	const float V_gambling = V_net * (V_unexposed / V_total) * StoneConfig->GamblingKCoefficient;
	const float V_final = FMath::Max(0.0f, V_net + V_gambling);

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

	// 当前回收价：已讨价锁定用锁价，否则随擦石进度实时算
	Info.CurrentValue = StoneData.bHaggleResolved ? StoneData.HaggleLockedPrice : CalculateSalePrice(StoneData);

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

void UClcStoneMarketSubsystem::DebugValidateGeneration(int32 Count)
{
	const int32 N = FMath::Max(1, Count);
	UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMarket][Validate] === 分布算法收敛自检 开始，N=%d ==="), N);

	int32 ConvergedCount = 0;
	int32 MaxDelta = 0;
	int32 WorstSeed = 0;
	float AvgJade = 0.0f, AvgCrack = 0.0f, AvgPatch = 0.0f;

	for (int32 i = 0; i < N; ++i)
	{
		bool bOK = false;
		FClcStoneInternalData Data = GenerateStoneInternal(bOK, 0.5f);
		if (!bOK) continue;

		AvgJade  += Data.GreenRatio;
		AvgCrack += Data.CrackRatio;
		AvgPatch += Data.LargestGreenPatchRatio;

		// 模拟“全开”：所有玉肉/杂质/裂纹都暴露
		FClcStoneRuntimeData RT;
		RT.Internal = Data;
		const float SA = Data.SurfaceArea;
		RT.AccumulatedOpenedArea = SA;
		RT.OpenedGreenArea    = SA * Data.GreenRatio;
		RT.OpenedImpurityArea = SA * Data.ImpurityRatio;
		RT.OpenedCrackArea    = SA * Data.CrackRatio;
		RT.OpenedBlackArea    = RT.OpenedImpurityArea + RT.OpenedCrackArea;
		RT.LargestExposedGreenPatch = SA * Data.GreenRatio * Data.LargestGreenPatchRatio;

		const int32 SaleFull = CalculateSalePrice(RT);
		const int32 Theo = FMath::RoundToInt(Data.TheoreticalValue);
		const int32 Delta = FMath::Abs(SaleFull - Theo);

		FString GradeName = TEXT("?");
		if (const UEnum* E = StaticEnum<EClcJadeGrade>())
		{
			GradeName = E->GetDisplayNameTextByValue(static_cast<int32>(Data.Grade)).ToString();
		}

		UE_LOG(LogClaudeCore, Warning,
			TEXT("[Validate] Seed=%d [%s] | 玉=%.1f%% 裂=%.1f%% 最大连续玉=%.0f%% | 全开售价=%d 理论=%d Δ=%d"),
			Data.Seed, *GradeName,
			Data.GreenRatio * 100.0f, Data.CrackRatio * 100.0f,
			Data.LargestGreenPatchRatio * 100.0f,
			SaleFull, Theo, Delta);

		if (Delta <= 1) ++ConvergedCount; // 四舍五入容差
		if (Delta > MaxDelta) { MaxDelta = Delta; WorstSeed = Data.Seed; }
	}

	UE_LOG(LogClaudeCore, Warning,
		TEXT("[ClcMarket][Validate] 平均：玉=%.1f%% 裂=%.1f%% 最大连续玉=%.0f%%"),
		AvgJade * 100.0f / N, AvgCrack * 100.0f / N, AvgPatch * 100.0f / N);
	UE_LOG(LogClaudeCore, Warning,
		TEXT("[ClcMarket][Validate] 收敛 %d/%d（全开售价==理论值，Δ≤1）；最大Δ=%d @Seed=%d"),
		ConvergedCount, N, MaxDelta, WorstSeed);
	UE_LOG(LogClaudeCore, Warning, TEXT("[ClcMarket][Validate] === 自检结束 ==="));
}
