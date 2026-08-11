// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "ClcLog.h"
#include "Data/ClcStoneConfig.h"
#include "Data/ClcStoneVoxelField3D.h"
#include "Math/Box.h"
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

	/**
	 * 切块尺寸惩罚因子——按切下块体积占原石总体积的比例 r 平滑压缩玉肉单价。
	 * r ∈ [Min,Max] → 1.0（标准，不压缩）；r<Min 从 1.0 衰减到 Undersized；r>Max 从 1.0 衰减到 Oversized。
	 * 用 smoothstep 过渡避免边界跳变。裂纹惩罚不缩（该罚照罚）。
	 */
	/**
	 * 切块尺寸惩罚因子——双向 smoothstep，约束区间 [0, 0.5]。
	 * < MinCutRatioForValue → 0.0；MinCut→Min → smoothstep(Undersized→1)；Min～Max → 1.0；
	 * Max→0.5 → smoothstep(1→Oversized)；>0.5 → Oversized。
	 */
	float ComputeCutSizeFactor(float Ratio, const UClcStoneConfig* Cfg)
	{
		if (!Cfg || Ratio < Cfg->MinCutRatioForValue) return 0.0f;
		const float Min   = Cfg->IdealCutRatioRange.X;
		const float Max   = Cfg->IdealCutRatioRange.Y;
		const float Under = Cfg->UndersizedSizeFactor;
		const float Over  = Cfg->OversizedSizeFactor;
		if (Min >= Max) return 1.0f;
		if (Ratio >= Min && Ratio <= Max) return 1.0f;
		if (Ratio < Min)
		{
			const float T = FMath::Clamp(Ratio / Min, 0.0f, 1.0f);
			const float Smooth = T * T * (3.0f - 2.0f * T);
			return FMath::Lerp(Under, 1.0f, Smooth);
		}
		// r>Max
		if (Ratio > 0.5f) return Over;
		const float T = FMath::Clamp((Ratio - Max) / FMath::Max(0.5f - Max, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		const float Smooth = T * T * (3.0f - 2.0f * T);
		return FMath::Lerp(1.0f, Over, Smooth);
	}

	/**
	 * 切块纯度与缺陷因子——解决"好坏通吃"。
	 * JadePurity = CutAwayJade / Total；DefectPenalty 按 Crack/Impurity/Waste 加权；
	 * PurityFactor = Clamp(JadePurity^Exponent × (1 - 0.8×DefectPenalty), 0, 1.2)。
	 */
	float ComputePurityFactor(int32 CutAwayJade, int32 CutAwayCrack,
		int32 CutAwayImpurity, int32 CutAwayTotal, const UClcStoneConfig* Cfg)
	{
		if (!Cfg || CutAwayTotal <= 0) return 1.0f;
		const float TotalF = static_cast<float>(CutAwayTotal);
		const float JadePurity = static_cast<float>(CutAwayJade) / TotalF;
		const int32 Waste = FMath::Max(0, CutAwayTotal - CutAwayJade - CutAwayCrack - CutAwayImpurity);
		const float DefectPenalty = (Cfg->CrackPenaltyWeight    * static_cast<float>(CutAwayCrack)
			+ Cfg->ImpurityPenaltyWeight * static_cast<float>(CutAwayImpurity)
			+ 0.3f * static_cast<float>(Waste)) / TotalF;
		const float PurityFactor = FMath::Pow(JadePurity, Cfg->PurityExponent)
			* FMath::Max(0.0f, 1.0f - 0.8f * DefectPenalty);
		return FMath::Clamp(PurityFactor, 0.0f, 1.2f);
	}

	/**
	 * 玉肉紧凑度因子——包围盒体积 vs 实际玉肉体积的比率。
	 * 紧凑=大片连续玉→溢价；松散=散碎玉→折扣。
	 * 无玉肉时返回 1.0f。
	 */
	float ComputeCompactnessFactor(int32 CutAwayJade, float VoxelVolume, const FBox& JadeBBox)
	{
		if (CutAwayJade <= 0) return 1.0f;
		const float PieceJadeVol = static_cast<float>(CutAwayJade) * VoxelVolume;
		const float BBoxVol = FMath::Max(1.0f, JadeBBox.IsValid ? JadeBBox.GetVolume() : 1.0f);
		const float Compactness = FMath::Clamp(PieceJadeVol / BBoxVol, 0.0f, 1.0f);
		return FMath::Lerp(0.7f, 1.1f, Compactness);
	}

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
// 定价公式 V3 —— 乘法衰减模型
// T = V_weighted × (1 - Clamp(α×CrackRatio + β×ImpurityRatio, 0, MaxDecayRatio))
// 数学保证 T > 0 恒成立（最差留下 (1-MaxDecayRatio)=5% 残值），不再需要 runtime soft-cap。
// ============================================================

float UClcStoneMarketSubsystem::CalculateTheoreticalValue(const FClcStoneInternalData& Data) const
{
	if (!StoneConfig) return 0.0f;

	// 玉肉（用实测占比）
	const float S_jade = Data.SurfaceArea * Data.GreenRatio;
	const float S_largest = S_jade * Data.LargestGreenPatchRatio;

	// 基础价值（含大块连续暴击）
	float V_exposed;
	if (S_largest > ContinuityAreaThreshold)
	{
		V_exposed = ((S_jade - S_largest) + S_largest * ContinuityBonusFactor) * StoneConfig->PricePerUnitArea;
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

	// 乘法衰减模型：瑕疵按比例打折，而非减法扣分。
	// CrackRatio ∈ [5%, 85%] 宽范围下减法公式大宗 T=0；乘法保证 T ≥ V_weighted × 5%。
	const float DecayRatio = FMath::Clamp(
		StoneConfig->CrackDecayWeight    * Data.CrackRatio +
		StoneConfig->ImpurityDecayWeight * Data.ImpurityRatio,
		0.0f, StoneConfig->MaxDecayRatio);

	return V_weighted * (1.0f - DecayRatio);
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
	if (S_opened <= 0.0f)
	{
		return FMath::RoundToInt(I.TheoreticalValue * StoneConfig->UnopenedFloorDiscountFactor);
	}

	// ---- 已暴露基础价值（玉肉 + 连续性暴击） ----
	const float S_green = StoneData.OpenedGreenArea;
	const float S_largest = StoneData.LargestExposedGreenPatch;

	float V_exposed;
	if (S_largest > ContinuityAreaThreshold)
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

	// 乘法衰减模型：已开区域按瑕疵比例打折（与理论价同源，保证全开收敛）
	const float OpenedCrackRatio = S_opened > 0.0f
		? (StoneData.OpenedCrackArea + StoneData.OpenedImpurityArea) / S_opened
		: 0.0f;
	const float DecayRatio = FMath::Clamp(
		StoneConfig->CrackDecayWeight    * OpenedCrackRatio +  // 已开区域裂纹与杂质统一按占比衰减
		StoneConfig->ImpurityDecayWeight * 0.0f,  // 当前有机缺陷体模型 ImpurityRatio=0，保留参数为后续扩展
		0.0f, StoneConfig->MaxDecayRatio);

	const float V_net = V_weighted * (1.0f - DecayRatio);

	// 净外推赌价：把已开区域的净价值密度对称外推到未开区域，从第一窗就激活。
	// 富窗净价值高→正赌价（吹高）；穷窗净价值低/负→负赌价（快速走低）。
	// 全开时 S_unopened=0 → 赌价=0 → 回收价=净价值=理论价值（收敛不变）。
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
// 解石（3D 体积）定价 —— 方案1: 3D 体素场独立锚定
// B_3D = OriginalJadeVolume × PricePerUnitVolume × GradeMultiplier × CutValueMultiplier
// 切块价 = GrossPieceBudget × SizeFactor × PurityFactor × CompactnessFactor（不再 ×0.4）
// 薄片 SizeFactor=0 → 不消耗预算，份额留给后续切块
// 2D TheoreticalValue 仅作旧存档回退（OriginalJadeVolume=0 时 fallback）
// ============================================================

int32 UClcStoneMarketSubsystem::CalculateCutPieceValue(const FClcStoneRuntimeData& StoneData,
	int32 CutAwayTotal, int32 CutAwayJade,
	int32 CutAwayCrack, int32 CutAwayImpurity,
	float VoxelVolume, int32 TotalVoxels,
	const FBox& PieceJadeBoundingBox,
	int32& OutConsumedBudgetAfter) const
{
	OutConsumedBudgetAfter = StoneData.ConsumedCutBudget;

	if (!StoneConfig || CutAwayTotal <= 0 || TotalVoxels <= 0)
	{
		return 0;
	}

	// ---- 1. 预算锚点 B_3D（3D 体素场玉肉体积 × 单价 × 种水 × 放大系数） ----
	// 优先 3D（方案1）；OriginalJadeVolume=0（旧存档）时回退 2D T。
	const bool bHas3D = StoneData.OriginalJadeVolume > KINDA_SMALL_NUMBER;
	float B;
	if (bHas3D)
	{
		// 3D 独立定价：玉肉体积 × 单价 × 种水系数 × 放大
		const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
			? StoneConfig->GradeValueMultiplier
			: GetDefaultGradeValueMultipliers();
		const float* GradeMult = ValueMults.Find(StoneData.Internal.Grade);
		const float C_sw = GradeMult ? *GradeMult : 1.0f;
		B = StoneData.OriginalJadeVolume * StoneConfig->PricePerUnitVolume
			* C_sw * FMath::Max(0.0f, StoneConfig->CutValueMultiplier);
	}
	else
	{
		// 旧存档回退：2D T × 放大系数（兼容 2026-08-11 前数据）
		const float T = FMath::Max(0.0f, StoneData.Internal.TheoreticalValue);
		const float M = FMath::Max(0.0f, StoneConfig->CutValueMultiplier);
		B = T * M;
	}
	if (B <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	// ---- 2. 玉肉份额增量分配 ----
	const float PieceJade = static_cast<float>(CutAwayJade) * VoxelVolume;
	const float OriginalJade = StoneData.ExposedJadeVolume + StoneData.RemainingJadeVolume;
	if (OriginalJade <= 0.0f)
	{
		return 0;
	}

	const float RemovedBefore = FMath::Max(0.0f, StoneData.ExposedJadeVolume - PieceJade);
	const int32 TargetBefore = FMath::RoundToInt(
		static_cast<float>(B) * RemovedBefore / OriginalJade);
	const int32 TargetAfter = FMath::RoundToInt(
		static_cast<float>(B) * StoneData.ExposedJadeVolume / OriginalJade);

	const int32 PriorConsumed = FMath::Max(StoneData.ConsumedCutBudget, TargetBefore);
	const int32 GrossPieceBudget = FMath::Max(0, TargetAfter - PriorConsumed);

	// ---- 3. 三大修正因子 ----
	const float Ratio = TotalVoxels > 0
		? static_cast<float>(CutAwayTotal) / static_cast<float>(TotalVoxels)
		: 0.0f;
	const float SizeFactor = ComputeCutSizeFactor(Ratio, StoneConfig);

	// 薄片（SizeFactor=0）：不消耗预算，份额留给后续切块（方案1修正）
	if (SizeFactor <= 0.0f)
	{
		OutConsumedBudgetAfter = StoneData.ConsumedCutBudget;
		return 0;
	}

	const float PurityFactor = ComputePurityFactor(
		CutAwayJade, CutAwayCrack, CutAwayImpurity, CutAwayTotal, StoneConfig);
	const float CompactnessFactor = ComputeCompactnessFactor(
		CutAwayJade, VoxelVolume, PieceJadeBoundingBox);

	// ---- 4. 结算（方案1: 废除 RoughStoneDiscount 全局折扣） ----
	OutConsumedBudgetAfter = FMath::Max(PriorConsumed, TargetAfter);
	const float DynamicValue = static_cast<float>(GrossPieceBudget)
		* SizeFactor * PurityFactor * CompactnessFactor;
	const int32 PieceGold = FMath::RoundToInt(DynamicValue);

	return PieceGold;
}

int32 UClcStoneMarketSubsystem::CalculateCutStoneSalePrice(const FClcStoneRuntimeData& StoneData) const
{
	// 方案1: 3D 独立定价——剩余主体回收价 = B_3D × 剩余玉肉份额 × 历史纯度溢价
	const bool bHas3D = StoneData.OriginalJadeVolume > KINDA_SMALL_NUMBER;
	const float TotalValue = bHas3D
		? [&]()
		{
			const TMap<EClcJadeGrade, float>& ValueMults = StoneConfig->GradeValueMultiplier.Num() > 0
				? StoneConfig->GradeValueMultiplier
				: GetDefaultGradeValueMultipliers();
			const float* GM = ValueMults.Find(StoneData.Internal.Grade);
			return StoneData.OriginalJadeVolume * StoneConfig->PricePerUnitVolume
				* (GM ? *GM : 1.0f) * FMath::Max(0.0f, StoneConfig->CutValueMultiplier);
		}()
		: FMath::Max(0.0f, StoneData.Internal.TheoreticalValue);

	const float OriginalJade = StoneData.ExposedJadeVolume + StoneData.RemainingJadeVolume;
	if (OriginalJade <= 0.0f) return 0;

	const float RemainingRatio = StoneData.RemainingJadeVolume / OriginalJade;

	// 历史已切开部分的累计纯度
	float HistoricalPurity = 0.5f;
	if (StoneData.ExposedCutVolume > 0.0f)
	{
		HistoricalPurity = FMath::Clamp(
			StoneData.ExposedJadeVolume / StoneData.ExposedCutVolume, 0.0f, 1.0f);
	}

	// 纯度溢价乘子
	const float PremiumMultiplier = FMath::Lerp(0.7f, 1.3f, HistoricalPurity);

	// 方案1: 废除 RoughStoneDiscount 全局折扣
	return FMath::RoundToInt(TotalValue * RemainingRatio * PremiumMultiplier);
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
	Info.DisplayName   = StoneData.DisplayName;
	Info.Origin        = StoneData.Internal.Origin;
	Info.bOpenedToJade = StoneData.OpenedGreenArea > 0.0f;
	Info.CurrentValue  = CalculateSalePrice(StoneData);
	Info.PurchasePrice = StoneData.Internal.PurchasePrice;

	if (Info.bOpenedToJade)
	{
		switch (StoneData.Internal.Grade)
		{
		case EClcJadeGrade::Bean:      Info.GradeText = TEXT("豆种"); break;
		case EClcJadeGrade::Glutinous: Info.GradeText = TEXT("糯种"); break;
		case EClcJadeGrade::Ice:       Info.GradeText = TEXT("冰种"); break;
		case EClcJadeGrade::Glass:     Info.GradeText = TEXT("玻璃种"); break;
		default: break;
		}
	}
	else
	{
		Info.ShellName = FString::Printf(TEXT("皮壳 #%d"), StoneData.Internal.ShellTypeIndex);
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

		// 模拟"全开"：所有玉肉/杂质/裂纹都暴露
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

		if (Delta <= 1) ++ConvergedCount;
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
