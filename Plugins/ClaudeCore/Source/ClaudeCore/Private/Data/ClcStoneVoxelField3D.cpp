// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcStoneVoxelField3D.h"
#include "Data/ClcJadeTypes.h"          // EClcDistVoxel: HostWaste/JadeBody/Impurity/Crack
#include "Math/RandomStream.h"
#include "Engine/StaticMesh.h"          // UStaticMesh + FStaticMeshRenderData/FStaticMeshLODResources + FBoxSphereBounds
#include "Rendering/PositionVertexBuffer.h"

namespace
{
	// ---- 确定性 RNG 辅助（与 2D 版 ClcJadeTypes.cpp 同构） ----

	int32 MakeSubSeed(int32 BaseSeed, int32 SubIndex)
	{
		return BaseSeed * 1103515245 + SubIndex;
	}

	int32 RandRange(FRandomStream& Rng, int32 Min, int32 Max)
	{
		return Min + Rng.RandHelper(Max - Min + 1);
	}

	// ---- 体素-三角形相交：Ericson 最近点-三角形 ----

	/** 点 P 到三角形 ABC 的最近点（Real-Time Collision Detection 5.1.5） */
	FVector ClosestPointOnTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector AB = B - A;
		const FVector AC = C - A;
		const FVector AP = P - A;

		const float D1 = FVector::DotProduct(AB, AP);
		const float D2 = FVector::DotProduct(AC, AP);
		if (D1 <= 0.0f && D2 <= 0.0f) return A; // AB & AC behind

		const FVector BP = P - B;
		const float D3 = FVector::DotProduct(AB, BP);
		const float D4 = FVector::DotProduct(AC, BP);
		if (D3 >= 0.0f && D4 <= D3) return B; // B region

		const float VC = D1 * D4 - D3 * D2;
		if (VC <= 0.0f && D1 >= 0.0f && D3 <= 0.0f)
		{
			const float Den = (D1 - D3);
			const float V = FMath::IsNearlyZero(Den) ? 0.0f : (D1 / Den);
			return A + V * AB; // edge AB
		}

		const FVector CP = P - C;
		const float D5 = FVector::DotProduct(AB, CP);
		const float D6 = FVector::DotProduct(AC, CP);
		if (D6 >= 0.0f && D5 <= D6) return C; // C region

		const float VA = D3 * D6 - D5 * D4;
		if (VA <= 0.0f && (D4 - D3) >= 0.0f && (D5 - D6) >= 0.0f)
		{
			const float Den = (D4 - D3) + (D5 - D6);
			const float W = FMath::IsNearlyZero(Den) ? 0.0f : ((D4 - D3) / Den);
			return B + W * (C - B); // edge BC
		}

		const float VB = D5 * D2 - D1 * D6;
		if (VB <= 0.0f && D2 >= 0.0f && D6 <= 0.0f)
		{
			const float Den = (D2 - D6);
			const float W = FMath::IsNearlyZero(Den) ? 0.0f : (D2 / Den);
			return A + W * AC; // edge AC
		}

		const float Denom = (VA + VB + VC);
		if (FMath::IsNearlyZero(Denom)) return A; // 退化三角形兜底
		const float Inv = 1.0f / Denom;
		const float Vp = VB * Inv;
		const float Wp = VC * Inv;
		return A + AB * Vp + AC * Wp; // face interior
	}

	/** 体素中心到三角形距离 < 阈值 → 该体素为表面 */
	inline bool VoxelTouchesTriangle(const FVector& VoxelCenter, const FVector& A, const FVector& B, const FVector& C, float DistSq)
	{
		const FVector Closest = ClosestPointOnTriangle(VoxelCenter, A, B, C);
		return FVector::DistSquared(Closest, VoxelCenter) <= DistSq;
	}

	// ---- 缺陷体生成（3D 版有机缺陷体） ----

	/** 盖一个填充球（缺陷体的一个体素节），覆盖玉肉，不重复盖已有缺陷。只在 mesh 内部盖。 */
	int32 StampBall(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res,
		int32 CX, int32 CY, int32 CZ, int32 R, FRandomStream& Rng)
	{
		if (R < 1) R = 1;
		int32 N = 0;
		const int32 R2 = R * R;
		const int32 InR2 = (R - 1) * (R - 1); // 内核半径平方（核内不抠边）
		for (int32 DZ = -R; DZ <= R; ++DZ)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					const int32 D2 = DX * DX + DY * DY + DZ * DZ;
					if (D2 > R2) continue;                                  // 球外
					const int32 X = CX + DX, Y = CY + DY, Z = CZ + DZ;
					if (X < 0 || X >= Res || Y < 0 || Y >= Res || Z < 0 || Z >= Res) continue;
					const int32 Idx = (Z * Res + Y) * Res + X;
					if (!Occ[Idx]) continue;                                // mesh 外不盖
					if (R >= 2 && D2 > InR2 && Rng.FRand() < 0.35f) continue; // 边缘随机抠
					if (Data[Idx] != Crack) { Data[Idx] = Crack; ++N; }
				}
			}
		}
		return N;
	}

	/**
	 * 沿 Dir 方向生长一条会分叉、半径向尖端递减的 3D 手臂（闪电/触手）。
	 * 预算 Budget 用完或步数到顶即停。返回新盖体素数。
	 */
	int32 GrowArm3D(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res, FRandomStream& Rng,
		float X, float Y, float Z, FVector Dir, int32 Radius, int32 Budget,
		float BranchProb, int32 Depth, int32 MaxDepth)
	{
		int32 Placed = 0;
		int32 Step = 0;
		constexpr int32 MaxSteps = 64;
		if (Dir.IsNearlyZero()) Dir = FVector(1, 0, 0);
		Dir.Normalize();

		while (Step < MaxSteps && Placed < Budget && Radius >= 1)
		{
			++Step;
			X += Dir.X; Y += Dir.Y; Z += Dir.Z;
			// 自然弯曲：随机扰动方向后重新归一化
			Dir += FVector(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f, Rng.FRand() - 0.5f) * 0.55f;
			Dir.Normalize();

			const int32 IX = FMath::RoundToInt(X);
			const int32 IY = FMath::RoundToInt(Y);
			const int32 IZ = FMath::RoundToInt(Z);
			if (IX < 0 || IX >= Res || IY < 0 || IY >= Res || IZ < 0 || IZ >= Res) break;

			Placed += StampBall(Data, Occ, Res, IX, IY, IZ, Radius, Rng);

			if (Step % 2 == 0) Radius = FMath::Max(1, Radius - 1); // 向尖端变细

			// 分叉：生成有连接关系的分支网络
			if (Depth < MaxDepth && Rng.FRand() < BranchProb && Budget - Placed > 16)
			{
				FVector BranchDir = Dir + FVector(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f, Rng.FRand() - 0.5f)
					* (1.0f + Rng.FRand() * 0.8f);
				BranchDir.Normalize();
				Placed += GrowArm3D(Data, Occ, Res, Rng, X, Y, Z, BranchDir,
					FMath::Max(1, Radius - 1), Budget - Placed, BranchProb, Depth + 1, MaxDepth);
			}
		}
		return Placed;
	}

	/**
	 * 在 (CX,CY,CZ) 生长一个连续不规则 3D 缺陷体。体积（TargetVoxels）决定形态：
	 *   小 → 蛛网/闪电（根半径1、多臂、高分叉，3D Fibonacci 球面布臂）；
	 *   大 → 团球伸出触手（粗根团球 + 少而粗的放射触手）。
	 */
	int32 GrowOrganism3D(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res, FRandomStream& Rng,
		int32 CX, int32 CY, int32 CZ, int32 TargetVoxels)
	{
		if (TargetVoxels <= 0) return 0;

		const int32 SmallBudget = FMath::RoundToInt(static_cast<float>(Res) * Res * Res * 0.012f);
		const bool bSmall = TargetVoxels < SmallBudget;

		const int32 RootR = bSmall ? 1
			: FMath::Clamp(FMath::RoundToInt(FMath::Pow(static_cast<float>(TargetVoxels), 1.0f / 3.0f) * 0.13f), 2, 9);
		const int32 NumArms = bSmall ? RandRange(Rng, 5, 9) : RandRange(Rng, 3, 5);
		const float BranchProb = bSmall ? 0.32f : 0.18f;

		int32 Placed = 0;
		if (!bSmall)
		{
			Placed += StampBall(Data, Occ, Res, CX, CY, CZ, RootR, Rng); // 团球核心
		}

		for (int32 A = 0; A < NumArms && Placed < TargetVoxels; ++A)
		{
			// Fibonacci 球面点 + 随机扰动：3D 均匀布臂数（替代 2D 圆周分布）
			const float T = (A + 0.5f) / static_cast<float>(NumArms);
			const float Phi = FMath::Acos(1.0f - 2.0f * T);
			const float Theta = PI * (1.0f + FMath::Sqrt(5.0f)) * A;
			FVector Dir(FMath::Sin(Phi) * FMath::Cos(Theta),
				FMath::Sin(Phi) * FMath::Sin(Theta),
				FMath::Cos(Phi));
			Dir += FVector(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f, Rng.FRand() - 0.5f) * 0.7f;
			Dir.Normalize();
			Placed += GrowArm3D(Data, Occ, Res, Rng, static_cast<float>(CX), static_cast<float>(CY), static_cast<float>(CZ),
				Dir, RootR, TargetVoxels - Placed, BranchProb, 0, 2);
		}
		return Placed;
	}

	/** 生成 Count 个 3D 缺陷体，覆盖率趋向 TargetCoverage；每个体分到的体积随机。 */
	int32 PlaceOrganisms3D(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res, FRandomStream& Rng,
		int32 Count, float TargetCoverage)
	{
		const int32 Total = Res * Res * Res;
		const int32 TargetVoxels = FMath::RoundToInt(static_cast<float>(Total) * FMath::Clamp(TargetCoverage, 0.0f, 0.95f));
		if (Count < 1) Count = 1;

		TArray<float> Weights;
		Weights.SetNumZeroed(Count);
		float WSum = 0.0f;
		for (int32 I = 0; I < Count; ++I) { Weights[I] = Rng.FRand() + 0.15f; WSum += Weights[I]; }

		int32 Placed = 0;
		for (int32 I = 0; I < Count && Placed < TargetVoxels; ++I)
		{
			const int32 Budget = FMath::RoundToInt(static_cast<float>(TargetVoxels) * (Weights[I] / WSum));
			// 找一个 mesh 内部体素作种子（最多试 8 次）
			int32 SX = 0, SY = 0, SZ = 0;
			bool bFound = false;
			for (int32 Try = 0; Try < 8 && !bFound; ++Try)
			{
				SX = RandRange(Rng, 2, Res - 3);
				SY = RandRange(Rng, 2, Res - 3);
				SZ = RandRange(Rng, 2, Res - 3);
				if (Occ[(SZ * Res + SY) * Res + SX]) bFound = true;
			}
			if (!bFound) continue;
			Placed += GrowOrganism3D(Data, Occ, Res, Rng, SX, SY, SZ, Budget);
		}
		return Placed;
	}

	/** BFS 计算剩余体中最大玉肉连通域体素数（6 邻域） */
	int32 MeasureLargestJadePatch3DImpl(const TArray<uint8>& Data, const TArray<uint8>& Occ,
		const TArray<uint8>& Remove, int32 Res)
	{
		const int32 Total = Res * Res * Res;
		TArray<bool> Visited;
		Visited.Init(false, Total);

		int32 MaxComponent = 0;
		for (int32 Z = 0; Z < Res; ++Z)
		for (int32 Y = 0; Y < Res; ++Y)
		for (int32 X = 0; X < Res; ++X)
		{
			const int32 Idx = (Z * Res + Y) * Res + X;
			if (!Occ[Idx] || Remove[Idx] || Data[Idx] != JadeBody || Visited[Idx]) continue;

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
				const int32 CY = (CurIdx / Res) % Res;
				const int32 CZ = CurIdx / (Res * Res);
				const int32 Nbrs[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
				for (const auto& D : Nbrs)
				{
					const int32 NX = CX + D[0], NY = CY + D[1], NZ = CZ + D[2];
					if (NX < 0 || NX >= Res || NY < 0 || NY >= Res || NZ < 0 || NZ >= Res) continue;
					const int32 NIdx = (NZ * Res + NY) * Res + NX;
					if (!Visited[NIdx] && Occ[NIdx] && !Remove[NIdx] && Data[NIdx] == JadeBody)
					{
						Visited[NIdx] = true;
						Queue.Add(NIdx);
					}
				}
			}
			MaxComponent = FMath::Max(MaxComponent, ComponentSize);
		}
		return MaxComponent;
	}

	// ---- mesh 体素化 ----

	/** 把 Field 的 OccupancyMask 按 mesh 实际形状填充（表面标记 + 外部 flood-fill） */
	void VoxelizeMeshInto(FClcStoneVoxelField3D& Field, UStaticMesh* Mesh)
	{
		const int32 Res = Field.Resolution;
		const int32 Total = Res * Res * Res;
		Field.OccupancyMask.Init(0, Total);

		const FBoxSphereBounds Bounds = Mesh ? Mesh->GetBounds()
			: FBoxSphereBounds(FVector::ZeroVector, FVector(50.0, 50.0, 50.0), 87.0f);
		Field.GridOrigin = Bounds.Origin - Bounds.BoxExtent;     // mesh local-space min
		Field.GridExtent = Bounds.BoxExtent * 2.0f;              // mesh local-space size
		Field.GridExtent = FVector(FMath::Max(Field.GridExtent.X, 1.0f), FMath::Max(Field.GridExtent.Y, 1.0f), FMath::Max(Field.GridExtent.Z, 1.0f)); // 防零
		Field.VoxelSize = Field.GridExtent / static_cast<float>(Res);
		Field.VoxelVolume = Field.VoxelSize.X * Field.VoxelSize.Y * Field.VoxelSize.Z;

		FStaticMeshRenderData* RD = Mesh ? Mesh->GetRenderData() : nullptr;
		if (!RD || RD->LODResources.Num() == 0)
		{
			// 无三角形数据 → 退化为包络椭球占用（flood-fill 退路也用同模型）
			const FVector InvExtent(1.0f / (Field.GridExtent.X * 0.5f), 1.0f / (Field.GridExtent.Y * 0.5f), 1.0f / (Field.GridExtent.Z * 0.5f));
			for (int32 Z = 0; Z < Res; ++Z)
			for (int32 Y = 0; Y < Res; ++Y)
			for (int32 X = 0; X < Res; ++X)
			{
				const FVector P = Field.VoxelToLocal(X, Y, Z) - Bounds.Origin; // 相对中心
				const FVector N(P.X * InvExtent.X, P.Y * InvExtent.Y, P.Z * InvExtent.Z);
				if (N.SizeSquared() <= 1.0f) Field.OccupancyMask[(Z * Res + Y) * Res + X] = 1;
			}
			return;
		}

		const FStaticMeshLODResources& LOD = RD->LODResources[0];
		const FPositionVertexBuffer& PosVB = LOD.VertexBuffers.PositionVertexBuffer;
		const uint32 NumTriangles = LOD.IndexBuffer.GetNumIndices() / 3;

		// Step 1: 标记表面体素（体素中心到三角形距离 < ~半体素对角 → 表面）
		const float SurfDist = (Field.VoxelSize.X + Field.VoxelSize.Y + Field.VoxelSize.Z) * 0.5f;
		const float SurfDistSq = SurfDist * SurfDist;

		auto LocalToVoxelClamped = [&Field, Res](const FVector& P) -> FIntVector
		{
			return FIntVector(
				FMath::Clamp(FMath::FloorToInt((P.X - Field.GridOrigin.X) / Field.VoxelSize.X), 0, Res - 1),
				FMath::Clamp(FMath::FloorToInt((P.Y - Field.GridOrigin.Y) / Field.VoxelSize.Y), 0, Res - 1),
				FMath::Clamp(FMath::FloorToInt((P.Z - Field.GridOrigin.Z) / Field.VoxelSize.Z), 0, Res - 1));
		};

		for (uint32 Tri = 0; Tri < NumTriangles; ++Tri)
		{
			const uint32 I0 = LOD.IndexBuffer.GetIndex(Tri * 3);
			const uint32 I1 = LOD.IndexBuffer.GetIndex(Tri * 3 + 1);
			const uint32 I2 = LOD.IndexBuffer.GetIndex(Tri * 3 + 2);
			const FVector V0 = (FVector)PosVB.VertexPosition(I0);
			const FVector V1 = (FVector)PosVB.VertexPosition(I1);
			const FVector V2 = (FVector)PosVB.VertexPosition(I2);

			FIntVector MinV = LocalToVoxelClamped(FVector(
				FMath::Min(FMath::Min(V0.X, V1.X), V2.X), FMath::Min(FMath::Min(V0.Y, V1.Y), V2.Y), FMath::Min(FMath::Min(V0.Z, V1.Z), V2.Z)));
			FIntVector MaxV = LocalToVoxelClamped(FVector(
				FMath::Max(FMath::Max(V0.X, V1.X), V2.X), FMath::Max(FMath::Max(V0.Y, V1.Y), V2.Y), FMath::Max(FMath::Max(V0.Z, V1.Z), V2.Z)));
			MinV = FIntVector(FMath::Max(MinV.X - 1, 0), FMath::Max(MinV.Y - 1, 0), FMath::Max(MinV.Z - 1, 0));
			MaxV = FIntVector(FMath::Min(MaxV.X + 1, Res - 1), FMath::Min(MaxV.Y + 1, Res - 1), FMath::Min(MaxV.Z + 1, Res - 1));

			for (int32 Z = MinV.Z; Z <= MaxV.Z; ++Z)
			for (int32 Y = MinV.Y; Y <= MaxV.Y; ++Y)
			for (int32 X = MinV.X; X <= MaxV.X; ++X)
			{
				const int32 Idx = (Z * Res + Y) * Res + X;
				if (Field.OccupancyMask[Idx]) continue;
				const FVector VC = Field.VoxelToLocal(X, Y, Z);
				if (VoxelTouchesTriangle(VC, V0, V1, V2, SurfDistSq)) Field.OccupancyMask[Idx] = 1;
			}
		}

		// Step 2: 从 grid 边界 flood-fill 标外部（非表面可达边界 = 外部）
		TArray<uint8> Exterior;
		Exterior.Init(0, Total);
		TArray<int32> Stack;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 A = 0; A < Res; ++A)
			for (int32 B = 0; B < Res; ++B)
			{
				for (int32 Side : {0, 1})
				{
					int32 X = (Axis == 0) ? (Side == 0 ? 0 : Res - 1) : A;
					int32 Y = (Axis == 1) ? (Side == 0 ? 0 : Res - 1) : (Axis == 0 ? A : B);
					int32 Z = (Axis == 2) ? (Side == 0 ? 0 : Res - 1) : B;
					const int32 Idx = (Z * Res + Y) * Res + X;
					if (!Field.OccupancyMask[Idx] && !Exterior[Idx]) { Exterior[Idx] = 1; Stack.Push(Idx); }
				}
			}
		}
		while (Stack.Num() > 0)
		{
			const int32 Cur = Stack.Pop(EAllowShrinking::No);
			const int32 CX = Cur % Res;
			const int32 CY = (Cur / Res) % Res;
			const int32 CZ = Cur / (Res * Res);
			const int32 Nbrs[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
			for (const auto& D : Nbrs)
			{
				const int32 NX = CX + D[0], NY = CY + D[1], NZ = CZ + D[2];
				if (NX < 0 || NX >= Res || NY < 0 || NY >= Res || NZ < 0 || NZ >= Res) continue;
				const int32 NIdx = (NZ * Res + NY) * Res + NX;
				if (!Field.OccupancyMask[NIdx] && !Exterior[NIdx]) { Exterior[NIdx] = 1; Stack.Push(NIdx); }
			}
		}

		// Step 3: 外部=0（废），其余（表面+内部）=占用
		for (int32 I = 0; I < Total; ++I)
		{
			if (Exterior[I]) Field.OccupancyMask[I] = 0;
			else Field.OccupancyMask[I] = 1;
		}
	}
}

// ====================== FClcStoneVoxelField3D 成员 ======================

FClcStoneVoxelField3D FClcStoneVoxelField3D::Generate(int32 Seed, UStaticMesh* Mesh,
	int32 Resolution, int32 DefectCount, float TargetCoverage)
{
	FClcStoneVoxelField3D Result;
	Result.Resolution = FMath::Max(8, Resolution);
	const int32 Res = Result.Resolution;
	const int32 Total = Res * Res * Res;

	// 1. 按 mesh 实际形状体素化 → OccupancyMask + Grid 映射
	VoxelizeMeshInto(Result, Mesh);

	// 2. 默认整块内部为玉肉，外部为废肉
	Result.Data.Init(HostWaste, Total);
	for (int32 I = 0; I < Total; ++I)
	{
		if (Result.OccupancyMask[I]) Result.Data[I] = JadeBody;
	}
	Result.RemoveMask.Init(0, Total);

	// 3. 生成若干连续不规则 3D 缺陷体（形态随各自体积变化）
	FRandomStream Rng(MakeSubSeed(Seed, 0));
	PlaceOrganisms3D(Result.Data, Result.OccupancyMask, Res, Rng,
		FMath::Max(1, DefectCount), TargetCoverage);

	return Result;
}

FVector FClcStoneVoxelField3D::VoxelToLocal(int32 X, int32 Y, int32 Z) const
{
	return FVector(
		GridOrigin.X + (X + 0.5f) * VoxelSize.X,
		GridOrigin.Y + (Y + 0.5f) * VoxelSize.Y,
		GridOrigin.Z + (Z + 0.5f) * VoxelSize.Z);
}

void FClcStoneVoxelField3D::LocalToVoxelInt(const FVector& LocalPos, int32& OutX, int32& OutY, int32& OutZ) const
{
	OutX = FMath::FloorToInt((LocalPos.X - GridOrigin.X) / VoxelSize.X);
	OutY = FMath::FloorToInt((LocalPos.Y - GridOrigin.Y) / VoxelSize.Y);
	OutZ = FMath::FloorToInt((LocalPos.Z - GridOrigin.Z) / VoxelSize.Z);
	if (OutX < 0 || OutX >= Resolution) OutX = -1;
	if (OutY < 0 || OutY >= Resolution) OutY = -1;
	if (OutZ < 0 || OutZ >= Resolution) OutZ = -1;
}

int32 FClcStoneVoxelField3D::IndexOf(int32 X, int32 Y, int32 Z) const
{
	if (X < 0 || X >= Resolution || Y < 0 || Y >= Resolution || Z < 0 || Z >= Resolution) return -1;
	return (Z * Resolution + Y) * Resolution + X;
}

float FClcStoneVoxelField3D::SampleAtLocalPos(const FVector& LocalPos) const
{
	// 三线性插值采样（0~3 浮点；越界返回 0=HostWaste）
	int32 X0, Y0, Z0;
	LocalToVoxelInt(LocalPos, X0, Y0, Z0);
	if (X0 < 0 || Y0 < 0 || Z0 < 0) return 0.0f;
	const int32 X1 = FMath::Min(X0 + 1, Resolution - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, Resolution - 1);
	const int32 Z1 = FMath::Min(Z0 + 1, Resolution - 1);

	const float Tx = FMath::Clamp((LocalPos.X - GridOrigin.X) / VoxelSize.X - X0, 0.0f, 1.0f);
	const float Ty = FMath::Clamp((LocalPos.Y - GridOrigin.Y) / VoxelSize.Y - Y0, 0.0f, 1.0f);
	const float Tz = FMath::Clamp((LocalPos.Z - GridOrigin.Z) / VoxelSize.Z - Z0, 0.0f, 1.0f);

	auto Get = [&](int32 X, int32 Y, int32 Z) -> float
	{
		const int32 I = IndexOf(X, Y, Z);
		return (I < 0) ? 0.0f : static_cast<float>(Data[I]);
	};

	const float C000 = Get(X0, Y0, Z0), C100 = Get(X1, Y0, Z0);
	const float C010 = Get(X0, Y1, Z0), C110 = Get(X1, Y1, Z0);
	const float C001 = Get(X0, Y0, Z1), C101 = Get(X1, Y0, Z1);
	const float C011 = Get(X0, Y1, Z1), C111 = Get(X1, Y1, Z1);

	const float C00 = C000 + (C100 - C000) * Tx;
	const float C10 = C010 + (C110 - C010) * Tx;
	const float C01 = C001 + (C101 - C001) * Tx;
	const float C11 = C011 + (C111 - C011) * Tx;

	const float C0 = C00 + (C10 - C00) * Ty;
	const float C1 = C01 + (C11 - C01) * Ty;
	return C0 + (C1 - C0) * Tz;
}

FClcStoneVoxelField3D::FSliceCounts FClcStoneVoxelField3D::ComputeSliceCounts(const FVector& PlaneNormal, float PlaneDistance) const
{
	FSliceCounts R;
	FVector N = PlaneNormal.GetSafeNormal();
	if (N.IsNearlyZero()) return R;
	const int32 Total = Resolution * Resolution * Resolution;
	for (int32 I = 0; I < Total; ++I)
	{
		if (!OccupancyMask[I] || RemoveMask[I]) continue;
		// 体素中心到平面 signed distance
		const int32 X = I % Resolution;
		const int32 Y = (I / Resolution) % Resolution;
		const int32 Z = I / (Resolution * Resolution);
		const FVector P = VoxelToLocal(X, Y, Z);
		const float Sd = FVector::DotProduct(N, P) + PlaneDistance;

		const uint8 V = Data[I];
		if (Sd < 0.0f)
		{
			++R.NegTotal;
			if (V == JadeBody) ++R.NegJade;
			else if (V == Crack) ++R.NegCrack;
			else if (V == Impurity) ++R.NegImpurity;
		}
		else
		{
			++R.PosTotal;
			if (V == JadeBody) ++R.PosJade;
			else if (V == Crack) ++R.PosCrack;
			else if (V == Impurity) ++R.PosImpurity;
		}
	}
	return R;
}

void FClcStoneVoxelField3D::ApplyCut(const FVector& PlaneNormal, float PlaneDistance, bool bRemoveNegativeSide)
{
	FVector N = PlaneNormal.GetSafeNormal();
	if (N.IsNearlyZero()) return;
	const int32 Total = Resolution * Resolution * Resolution;
	for (int32 I = 0; I < Total; ++I)
	{
		if (!OccupancyMask[I] || RemoveMask[I]) continue;
		const int32 X = I % Resolution;
		const int32 Y = (I / Resolution) % Resolution;
		const int32 Z = I / (Resolution * Resolution);
		const FVector P = VoxelToLocal(X, Y, Z);
		const float Sd = FVector::DotProduct(N, P) + PlaneDistance;
		const bool bOnNeg = (Sd < 0.0f);
		if (bRemoveNegativeSide == bOnNeg) RemoveMask[I] = 1;
	}
}

void FClcStoneVoxelField3D::CountRemainingVoxels(int32& OutTotal, int32& OutJade, int32& OutCrack, int32& OutImpurity) const
{
	OutTotal = OutJade = OutCrack = OutImpurity = 0;
	const int32 Total = Resolution * Resolution * Resolution;
	for (int32 I = 0; I < Total; ++I)
	{
		if (!OccupancyMask[I] || RemoveMask[I]) continue;
		++OutTotal;
		const uint8 V = Data[I];
		if (V == JadeBody) ++OutJade;
		else if (V == Crack) ++OutCrack;
		else if (V == Impurity) ++OutImpurity;
	}
}

int32 FClcStoneVoxelField3D::MeasureLargestJadePatch3D() const
{
	return MeasureLargestJadePatch3DImpl(Data, OccupancyMask, RemoveMask, Resolution);
}

float FClcStoneVoxelField3D::GetRemainingVolume() const
{
	int32 Total = 0, J = 0, C = 0, I = 0;
	CountRemainingVoxels(Total, J, C, I);
	return static_cast<float>(Total) * VoxelVolume;
}

FString FClcStoneVoxelField3D::DebugDescribe() const
{
	int32 Total = 0, Jade = 0, CrackV = 0, Imp = 0;
	CountRemainingVoxels(Total, Jade, CrackV, Imp);
	const int32 OccCount = [this]()
	{
		int32 N = 0;
		for (uint8 b : OccupancyMask) if (b) ++N;
		return N;
	}();
	const int32 GridTotal = Resolution * Resolution * Resolution;
	const int32 Largest = MeasureLargestJadePatch3D();
	return FString::Printf(TEXT(
		"[VoxelField3D] Res=%d Occ=%d/%d(%.1f%%) | Remaining Total=%d Jade=%d Crack=%d Imp=%d | Vol=%.1fcm³ | LargestJade=%d (%.0f%% of jade)"),
		Resolution, OccCount, GridTotal, GridTotal > 0 ? 100.0 * OccCount / GridTotal : 0.0,
		Total, Jade, CrackV, Imp, GetRemainingVolume(),
		Largest, Jade > 0 ? 100.0 * Largest / Jade : 0.0);
}
