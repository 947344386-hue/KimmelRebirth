// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcJadeTypes.h"
#include "Math/RandomStream.h"
#include "Containers/Queue.h"

namespace
{
	/** 确定性哈希生成子种子（避免绿/黑/测量共享同一随机流） */
	int32 MakeSubSeed(int32 BaseSeed, int32 SubIndex)
	{
		return BaseSeed * 1103515245 + SubIndex;
	}

	/** 在 [Min, Max] 范围内取随机整数 */
	int32 RandRange(FRandomStream& Rng, int32 Min, int32 Max)
	{
		return Min + Rng.RandHelper(Max - Min + 1);
	}

	/** 检查像素是否在 UV 边界内（杂裂不能贴边） */
	constexpr int32 BorderMargin = 4;
	bool IsInBorder(int32 X, int32 Y, int32 Res)
	{
		return X <= BorderMargin || X >= Res - 1 - BorderMargin || Y <= BorderMargin || Y >= Res - 1 - BorderMargin;
	}

	/** 放置一个 blob：从种子点 BFS 扩展，直到达到目标像素数或 BFS 耗尽 */
	int32 GrowBlob(TArray<uint8>& Map, int32 Res, FRandomStream& Rng, int32 StartX, int32 StartY, uint8 PixelValue, int32 MaxPixels, bool bAllowBorder)
	{
		static const int32 Dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };

		TQueue<FIntPoint> Queue;

		int32 Placed = 0;

		// 标记起始像素（如果可写）
		{
			const int32 Idx = StartY * Res + StartX;
			if (Map[Idx] == 0 && (!IsInBorder(StartX, StartY, Res) || bAllowBorder))
			{
				Map[Idx] = PixelValue;
				Placed = 1;
				Queue.Enqueue(FIntPoint(StartX, StartY));
			}
		}

		// 如果起始像素无法放置（已占用或边界），直接返回 0
		if (Placed == 0) return 0;

		TSet<uint64> Visited; // pack x,y into uint64 for visited tracking
		Visited.Add(((uint64)StartX << 32) | (uint32)StartY);

		while (!Queue.IsEmpty() && Placed < MaxPixels)
		{
			FIntPoint P;
			Queue.Dequeue(P);

			// 打乱方向顺序，增加 blob 不规则感
			int32 DirOrder[4] = { 0,1,2,3 };
			for (int32 i = 0; i < 4; ++i)
			{
				const int32 j = RandRange(Rng, i, 3);
				Swap(DirOrder[i], DirOrder[j]);
			}

			for (int32 d = 0; d < 4 && Placed < MaxPixels; ++d)
			{
				const int32 NX = P.X + Dirs[DirOrder[d]][0];
				const int32 NY = P.Y + Dirs[DirOrder[d]][1];

				if (NX < 0 || NX >= Res || NY < 0 || NY >= Res) continue;

				const uint64 Key = ((uint64)NX << 32) | (uint32)NY;
				if (Visited.Contains(Key)) continue;
				Visited.Add(Key);

				const int32 NIdx = NY * Res + NX;

				if (Map[NIdx] != 0) continue;

				// 杂裂禁止贴边：不允许在边界区域放置黑像素
				if (!bAllowBorder && IsInBorder(NX, NY, Res)) continue;

				Map[NIdx] = PixelValue;
				++Placed;

				if (Placed >= MaxPixels) break;

				// 随机决定是否继续扩展此邻居（增加形状不规则性）
				if (Rng.FRand() < 0.85f)
				{
					Queue.Enqueue(FIntPoint(NX, NY));
				}
			}
		}

		return Placed;
	}

	/** BFS 计算最大绿色连通域像素数 */
	int32 MeasureLargestGreenPatch(const TArray<uint8>& Map, int32 Res)
	{
		TArray<bool> Visited;
		Visited.Init(false, Res * Res);

		int32 MaxComponent = 0;

		for (int32 Y = 0; Y < Res; ++Y)
		{
			for (int32 X = 0; X < Res; ++X)
			{
				const int32 Idx = Y * Res + X;
				if (Map[Idx] != 1 || Visited[Idx]) continue;

				// 新连通域，BFS 计数
				TQueue<FIntPoint> Queue;
				Queue.Enqueue(FIntPoint(X, Y));
				Visited[Idx] = true;

				int32 ComponentSize = 0;
				while (!Queue.IsEmpty())
				{
					FIntPoint P;
					Queue.Dequeue(P);
					++ComponentSize;

					// 4 邻域
					const int32 Nbrs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
					for (const auto& D : Nbrs)
					{
						const int32 NX = P.X + D[0];
						const int32 NY = P.Y + D[1];
						if (NX < 0 || NX >= Res || NY < 0 || NY >= Res) continue;
						const int32 NIdx = NY * Res + NX;
						if (!Visited[NIdx] && Map[NIdx] == 1)
						{
							Visited[NIdx] = true;
							Queue.Enqueue(FIntPoint(NX, NY));
						}
					}
				}

				MaxComponent = FMath::Max(MaxComponent, ComponentSize);
			}
		}

		return MaxComponent;
	}
}

FClcStoneDistributionMap FClcStoneDistributionMap::Generate(int32 Seed, float GreenRatio, float BlackRatio, float& OutActualLargestPatchRatio)
{
	FClcStoneDistributionMap Result;
	const int32 TotalPixels = Resolution * Resolution;

	// GreenRatio + BlackRatio = 1.0（无普通石头，全为玉/杂）
	const float Total = GreenRatio + BlackRatio;
	const float NormGreen = Total > 0.0f ? GreenRatio / Total : 0.5f;
	const int32 TargetGreen = FMath::RoundToInt(TotalPixels * FMath::Clamp(NormGreen, 0.0f, 1.0f));

	FRandomStream Rng(MakeSubSeed(Seed, 0));
	FRandomStream RngBlack(MakeSubSeed(Seed, 1));

	// ---- 放置绿色 Blob ----
	int32 GreenPlaced = 0;
	int32 Attempts = 0;
	const int32 MaxAttempts = TargetGreen * 3 + 200;

	while (GreenPlaced < TargetGreen && Attempts < MaxAttempts)
	{
		const int32 SX = RandRange(Rng, 2, Resolution - 3);
		const int32 SY = RandRange(Rng, 2, Resolution - 3);

		if (Result.Data[SY * Resolution + SX] == 0)
		{
			const int32 BlobCap = FMath::Min(TargetGreen - GreenPlaced, RandRange(Rng, 12, 200));
			GreenPlaced += GrowBlob(Result.Data, Resolution, Rng, SX, SY, 1, BlobCap, true);
		}
		++Attempts;
	}

	// ---- 剩余全归为杂裂(2) —— 无普通石头 ----
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Idx = Y * Resolution + X;
			if (Result.Data[Idx] == 0)
			{
				Result.Data[Idx] = 2;
			}
		}
	}

	// ---- BFS 测量最大连续绿块 ----
	const int32 LargestGreenPatchPixels = MeasureLargestGreenPatch(Result.Data, Resolution);
	OutActualLargestPatchRatio = (GreenPlaced > 0)
		? static_cast<float>(LargestGreenPatchPixels) / static_cast<float>(GreenPlaced)
		: 0.0f;

	return Result;
}
