// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcJadeTypes.h"
#include "Math/RandomStream.h"

namespace
{
	/** 确定性子种子 */
	int32 MakeSubSeed(int32 BaseSeed, int32 SubIndex)
	{
		return BaseSeed * 1103515245 + SubIndex;
	}

	/** 在 [Min, Max] 范围内取随机整数 */
	int32 RandRange(FRandomStream& Rng, int32 Min, int32 Max)
	{
		return Min + Rng.RandHelper(Max - Min + 1);
	}

	/** 缺陷体禁止贴 UV 边（避免形成黑环） */
	constexpr int32 BorderMargin = 4;

	/**
	 * 盖一个填充圆盘（缺陷体的一个体素节），覆盖玉肉，不重复盖已有缺陷。
	 * 边缘随机抠掉一点，让边界更不规则（更像自然杂裂）。返回新盖像素数。
	 */
	int32 StampDisk(TArray<uint8>& Map, int32 Res, int32 CX, int32 CY, int32 R, FRandomStream& Rng)
	{
		if (R < 1) R = 1;
		int32 N = 0;
		const int32 R2 = R * R;
		const int32 InR2 = (R - 1) * (R - 1); // 内核半径平方（核内不抠边）
		for (int32 DY = -R; DY <= R; ++DY)
		{
			const int32 Y = CY + DY;
			if (Y <= BorderMargin || Y >= Res - 1 - BorderMargin) continue;
			for (int32 DX = -R; DX <= R; ++DX)
			{
				const int32 X = CX + DX;
				if (X <= BorderMargin || X >= Res - 1 - BorderMargin) continue;
				const int32 D2 = DX * DX + DY * DY;
				if (D2 > R2) continue;                                  // 圆盘外
				if (R >= 2 && D2 > InR2 && Rng.FRand() < 0.35f) continue; // 边缘随机抠
				const int32 Idx = Y * Res + X;
				if (Map[Idx] != Crack) { Map[Idx] = Crack; ++N; }
			}
		}
		return N;
	}

	/**
	 * 沿 Ang 方向生长一条会分叉、半径向尖端递减的手臂（闪电/触手）。
	 * 预算 Budget 用完或步数到顶即停。返回新盖像素数。
	 */
	int32 GrowArm(TArray<uint8>& Map, int32 Res, FRandomStream& Rng,
		float X, float Y, float Ang, int32 Radius, int32 Budget,
		float BranchProb, int32 Depth, int32 MaxDepth)
	{
		int32 Placed = 0;
		int32 Step = 0;
		constexpr int32 MaxSteps = 64;
		while (Step < MaxSteps && Placed < Budget && Radius >= 1)
		{
			++Step;
			X += FMath::Cos(Ang);
			Y += FMath::Sin(Ang);
			Ang += (Rng.FRand() - 0.5f) * 0.55f; // 自然弯曲

			const int32 IX = FMath::RoundToInt(X);
			const int32 IY = FMath::RoundToInt(Y);
			if (IX <= BorderMargin || IX >= Res - 1 - BorderMargin || IY <= BorderMargin || IY >= Res - 1 - BorderMargin) break;

			Placed += StampDisk(Map, Res, IX, IY, Radius, Rng);

			if (Step % 2 == 0) Radius = FMath::Max(1, Radius - 1); // 向尖端变细

			// 分叉：生成有连接关系的分支网络
			if (Depth < MaxDepth && Rng.FRand() < BranchProb && Budget - Placed > 16)
			{
				const float Off = (Rng.FRand() < 0.5f ? -1.0f : 1.0f) * (0.5f + Rng.FRand() * 0.6f);
				Placed += GrowArm(Map, Res, Rng, X, Y, Ang + Off, FMath::Max(1, Radius - 1),
					Budget - Placed, BranchProb, Depth + 1, MaxDepth);
			}
		}
		return Placed;
	}

	/**
	 * 在 (CX,CY) 生长一个连续不规则缺陷体。体积（TargetPixels）决定形态：
	 *   小 → 蛛网/闪电（根半径1、多臂、高分叉）；
	 *   大 → 团球伸出触手（粗根团球 + 少而粗的放射触手）。
	 */
	int32 GrowOrganism(TArray<uint8>& Map, int32 Res, FRandomStream& Rng,
		int32 CX, int32 CY, int32 TargetPixels)
	{
		if (TargetPixels <= 0) return 0;

		const int32 SmallBudget = FMath::RoundToInt(Res * Res * 0.012f); // ~750px：以下为“小体积”
		const bool bSmall = TargetPixels < SmallBudget;

		// 根半径：体积越大根部越粗（团球核心）
		const int32 RootR = bSmall ? 1 : FMath::Clamp(FMath::RoundToInt(FMath::Sqrt(static_cast<float>(TargetPixels)) * 0.13f), 2, 9);
		// 主臂数：小体积多方向（蛛网/闪电）；大体积少而粗的触手
		const int32 NumArms = bSmall ? RandRange(Rng, 5, 9) : RandRange(Rng, 3, 5);
		const float BranchProb = bSmall ? 0.32f : 0.18f;

		int32 Placed = 0;
		if (!bSmall)
		{
			Placed += StampDisk(Map, Res, CX, CY, RootR, Rng); // 团球核心
		}

		for (int32 A = 0; A < NumArms && Placed < TargetPixels; ++A)
		{
			const float Ang = (A / static_cast<float>(NumArms)) * PI * 2.0f + Rng.FRand() * 0.7f;
			Placed += GrowArm(Map, Res, Rng, static_cast<float>(CX), static_cast<float>(CY),
				Ang, RootR, TargetPixels - Placed, BranchProb, 0, 2);
		}
		return Placed;
	}

	/**
	 * 生成 Count 个缺陷体，覆盖率趋向 TargetCoverage。
	 * 每个体分到的体积随机（有的大有的小）→ 形态自然分化为蛛网/团球触手。
	 */
	int32 PlaceOrganisms(TArray<uint8>& Map, int32 Res, FRandomStream& Rng,
		int32 Count, float TargetCoverage)
	{
		const int32 Total = Res * Res;
		const int32 TargetPixels = FMath::RoundToInt(Total * FMath::Clamp(TargetCoverage, 0.0f, 0.95f));
		if (Count < 1) Count = 1;

		// 随机权重分配预算
		TArray<float> Weights;
		Weights.SetNumZeroed(Count);
		float WSum = 0.0f;
		for (int32 I = 0; I < Count; ++I)
		{
			Weights[I] = Rng.FRand() + 0.15f;
			WSum += Weights[I];
		}

		int32 Placed = 0;
		for (int32 I = 0; I < Count && Placed < TargetPixels; ++I)
		{
			const int32 Budget = FMath::RoundToInt(static_cast<float>(TargetPixels) * (Weights[I] / WSum));
			const int32 SX = RandRange(Rng, BorderMargin + 8, Res - 9 - BorderMargin);
			const int32 SY = RandRange(Rng, BorderMargin + 8, Res - 9 - BorderMargin);
			Placed += GrowOrganism(Map, Res, Rng, SX, SY, Budget);
		}
		return Placed;
	}

	/** BFS 计算最大玉肉连通域像素数（4 邻域）——缺陷体把玉切碎后，反映还剩多大一块连续玉。 */
	int32 MeasureLargestJadePatch(const TArray<uint8>& Map, int32 Res)
	{
		TArray<bool> Visited;
		Visited.Init(false, Res * Res);

		int32 MaxComponent = 0;
		for (int32 Y = 0; Y < Res; ++Y)
		{
			for (int32 X = 0; X < Res; ++X)
			{
				const int32 Idx = Y * Res + X;
				if (Map[Idx] != JadeBody || Visited[Idx]) continue;

				TArray<int32> Queue;
				Queue.Reserve(512);
				Queue.Add(Idx);
				Visited[Idx] = true;

				int32 ComponentSize = 0;
				int32 Head = 0;
				while (Head < Queue.Num())
				{
					const int32 CurIdx = Queue[Head++];
					++ComponentSize;

					const int32 CX = CurIdx % Res;
					const int32 CY = CurIdx / Res;
					const int32 Nbrs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
					for (const auto& D : Nbrs)
					{
						const int32 NX = CX + D[0];
						const int32 NY = CY + D[1];
						if (NX < 0 || NX >= Res || NY < 0 || NY >= Res) continue;
						const int32 NIdx = NY * Res + NX;
						if (!Visited[NIdx] && Map[NIdx] == JadeBody)
						{
							Visited[NIdx] = true;
							Queue.Add(NIdx);
						}
					}
				}
				MaxComponent = FMath::Max(MaxComponent, ComponentSize);
			}
		}
		return MaxComponent;
	}
}

FClcStoneDistributionMap::FMeasureResult FClcStoneDistributionMap::Measure() const
{
	FMeasureResult R;
	for (int32 I = 0; I < Data.Num(); ++I)
	{
		if (Data[I] == JadeBody) ++R.JadePixels;
		else if (Data[I] == Crack) ++R.CrackPixels;
	}
	R.LargestJadePatchPixels = MeasureLargestJadePatch(Data, Resolution);
	return R;
}

FClcStoneDistributionMap FClcStoneDistributionMap::Generate(int32 Seed, int32 DefectCount,
	float TargetCoverage, FMeasureResult& OutActuals)
{
	FClcStoneDistributionMap Result;
	const int32 Total = Resolution * Resolution;

	// 1. 默认整块都是玉肉
	for (int32 I = 0; I < Total; ++I)
	{
		Result.Data[I] = JadeBody;
	}

	// 2. 生成若干连续不规则缺陷体（形态随各自体积变化：蛛网/团球触手）
	FRandomStream Rng(MakeSubSeed(Seed, 0));
	PlaceOrganisms(Result.Data, Resolution, Rng, FMath::Max(1, DefectCount), TargetCoverage);

	// 3. 实测权威统计（定价只认这个）
	OutActuals = Result.Measure();
	return Result;
}
