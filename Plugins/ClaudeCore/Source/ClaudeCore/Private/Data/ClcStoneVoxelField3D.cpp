// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcStoneVoxelField3D.h"
#include "Data/ClcJadeTypes.h"          // EClcDistVoxel: HostWaste/JadeBody/Impurity/Crack
#include "ClcLog.h"
#include "Math/RandomStream.h"
#include "Engine/StaticMesh.h"          // UStaticMesh + FStaticMeshRenderData/FStaticMeshLODResources + FBoxSphereBounds
#include "Rendering/PositionVertexBuffer.h"

namespace
{
	// ---- 确定性 RNG 辅助（与 2D 版 ClcJadeTypes.cpp 同构，名称加 Voxel 前缀避免 Unity Build 冲突） ----

	FORCEINLINE int32 VoxelMakeSubSeed(int32 BaseSeed, int32 SubIndex)
	{
		return BaseSeed * 1103515245 + SubIndex;
	}

	FORCEINLINE int32 VoxelRandRange(FRandomStream& Rng, int32 Min, int32 Max)
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

	// ---- 3D 确定性噪声引擎（Permutation-based value noise，Seed 可复现） ----

	struct FVoxelNoise3D
	{
		uint8 Perm[512];

		void Init(int32 Seed)
		{
			FRandomStream Rng(VoxelMakeSubSeed(Seed, 999));
			for (int32 I = 0; I < 256; ++I) Perm[I] = static_cast<uint8>(I);
			// Fisher-Yates 洗牌
			for (int32 I = 255; I > 0; --I)
			{
				const int32 J = Rng.RandHelper(I + 1);
				Swap(Perm[I], Perm[J]);
			}
			// 双倍到 512 以简化 hash 包裹
			for (int32 I = 0; I < 256; ++I) Perm[256 + I] = Perm[I];
		}

		/** 3D value noise, smoothstep 插值, 输出 [0,1] */
		float Noise(float X, float Y, float Z) const
		{
			const int32 Xi = FMath::FloorToInt(X) & 255;
			const int32 Yi = FMath::FloorToInt(Y) & 255;
			const int32 Zi = FMath::FloorToInt(Z) & 255;
			const float Xf = X - FMath::FloorToInt(X);
			const float Yf = Y - FMath::FloorToInt(Y);
			const float Zf = Z - FMath::FloorToInt(Z);
			const float U = Xf * Xf * (3.0f - 2.0f * Xf);
			const float V = Yf * Yf * (3.0f - 2.0f * Yf);
			const float W = Zf * Zf * (3.0f - 2.0f * Zf);

			auto Hash = [this](int32 X, int32 Y, int32 Z) -> uint8
			{
				return Perm[Perm[Perm[X] + Y] + Z];
			};

			const float A = static_cast<float>(Hash(Xi, Yi, Zi)) / 255.0f;
			const float B = static_cast<float>(Hash(Xi + 1, Yi, Zi)) / 255.0f;
			const float C = static_cast<float>(Hash(Xi, Yi + 1, Zi)) / 255.0f;
			const float D = static_cast<float>(Hash(Xi + 1, Yi + 1, Zi)) / 255.0f;
			const float E = static_cast<float>(Hash(Xi, Yi, Zi + 1)) / 255.0f;
			const float F = static_cast<float>(Hash(Xi + 1, Yi, Zi + 1)) / 255.0f;
			const float G = static_cast<float>(Hash(Xi, Yi + 1, Zi + 1)) / 255.0f;
			const float H = static_cast<float>(Hash(Xi + 1, Yi + 1, Zi + 1)) / 255.0f;

			const float AB = A + (B - A) * U;
			const float CD = C + (D - C) * U;
			const float EF = E + (F - E) * U;
			const float GH = G + (H - G) * U;
			const float ABCD = AB + (CD - AB) * V;
			const float EFGH = EF + (GH - EF) * V;
			return ABCD + (EFGH - ABCD) * W;
		}
	};

	// ---- 新三步缺陷生成 ----

	/** 第一步：噪声扰动断层面裂纹。写入 EClcDistVoxel::Crack。
	 *  单条平面覆盖不足时自动循环生成多条交叉断层，直至 CrackBudget 耗尽。 */
	void GeneratePlanesOrStrands(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res,
		FRandomStream& Rng, const FVoxelNoise3D& Noise, int32& CrackBudget)
	{
		if (CrackBudget <= 0) return;

		// 边缘种子选择器（复用旧逻辑：优先 mesh 边缘）
		auto FindEdgeSeed = [&](int32& OutX, int32& OutY, int32& OutZ) -> bool
		{
			int32 BestX = -1, BestY = -1, BestZ = -1;
			int32 BestEdgeCount = -1;
			for (int32 Try = 0; Try < 12; ++Try)
			{
				const int32 CX = VoxelRandRange(Rng, 2, Res - 3);
				const int32 CY = VoxelRandRange(Rng, 2, Res - 3);
				const int32 CZ = VoxelRandRange(Rng, 2, Res - 3);
				const int32 Idx = (CZ * Res + CY) * Res + CX;
				if (!Occ[Idx]) continue;
				int32 Edge = 0;
				const int32 Nbrs[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
				for (const auto& D : Nbrs)
				{
					const int32 NX = CX + D[0], NY = CY + D[1], NZ = CZ + D[2];
					if (NX < 0 || NX >= Res || NY < 0 || NY >= Res || NZ < 0 || NZ >= Res) { ++Edge; continue; }
					if (!Occ[(NZ * Res + NY) * Res + NX]) ++Edge;
				}
				if (Edge > BestEdgeCount) { BestEdgeCount = Edge; BestX = CX; BestY = CY; BestZ = CZ; }
				if (Edge >= 2) break;
			}
			if (BestX < 0) return false;
			OutX = BestX; OutY = BestY; OutZ = BestZ;
			return true;
		};

		constexpr float NoiseFrequency = 0.1f;
		constexpr float NoiseAmplitude = 2.5f;
		constexpr float CrackHalfThickness = 1.5f;

		// 计算网格中心（供法向量生成用）
		int32 OccCount = 0;
		float Cx = 0.0f, Cy = 0.0f, Cz = 0.0f;
		for (int32 Z = 0; Z < Res; ++Z)
		for (int32 Y = 0; Y < Res; ++Y)
		for (int32 X = 0; X < Res; ++X)
		{
			if (Occ[(Z * Res + Y) * Res + X]) { Cx += X; Cy += Y; Cz += Z; ++OccCount; }
		}
		const FVector Center(Cx / OccCount, Cy / OccCount, Cz / OccCount);

		int32 PlaneIndex = 0;
		while (CrackBudget > 0 && PlaneIndex < 8) // 安全上限
		{
			++PlaneIndex;

			// 种子点与法向量
			int32 SX = 0, SY = 0, SZ = 0;
			FVector P0, N;
			if (FindEdgeSeed(SX, SY, SZ))
			{
				P0 = FVector(SX, SY, SZ);
			}
			else
			{
				// 回退：随机内部体素
				for (int32 Try = 0; Try < 50; ++Try)
				{
					const int32 X = VoxelRandRange(Rng, 2, Res - 3);
					const int32 Y = VoxelRandRange(Rng, 2, Res - 3);
					const int32 Z = VoxelRandRange(Rng, 2, Res - 3);
					if (Occ[(Z * Res + Y) * Res + X]) { P0 = FVector(X, Y, Z); break; }
				}
			}
			// 随机法向量（尽量偏向指向中心，让断层穿过石体）
			const FVector ToCenter = Center - P0;
			if (!ToCenter.IsNearlyZero())
			{
				N = ToCenter.GetSafeNormal() +
					FVector(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f, Rng.FRand() - 0.5f) * 0.8f;
			}
			else
			{
				N = FVector(Rng.FRand() - 0.5f, Rng.FRand() - 0.5f, Rng.FRand() - 0.5f);
			}
			N.Normalize();
			if (N.IsNearlyZero()) N = FVector(1.0f, 0.0f, 0.0f);

			// 扫描整个网格，写入 Crack
			for (int32 Z = 0; Z < Res && CrackBudget > 0; ++Z)
			for (int32 Y = 0; Y < Res && CrackBudget > 0; ++Y)
			for (int32 X = 0; X < Res && CrackBudget > 0; ++X)
			{
				const int32 Idx = (Z * Res + Y) * Res + X;
				if (!Occ[Idx]) continue;
				if (Data[Idx] != JadeBody) continue; // 不覆盖已有缺陷

				const FVector P(X, Y, Z);
				const float D = FVector::DotProduct(P - P0, N); // 到基准平面的距离
				const float DNoisy = D + (Noise.Noise(X * NoiseFrequency, Y * NoiseFrequency, Z * NoiseFrequency) - 0.5f) * 2.0f * NoiseAmplitude;

				if (FMath::Abs(DNoisy) < CrackHalfThickness)
				{
					Data[Idx] = Crack;
					--CrackBudget;
				}
			}
		}
	}

	/** 第二步：沿裂纹边缘浸染杂质。写入 EClcDistVoxel::Impurity。
	 *  遍历全部 Crack 体素的 26-邻域，对邻居 JadeBody 用噪声判定是否转为 Impurity。 */
	void GenerateImpurityDiffusion(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res,
		FRandomStream& Rng, const FVoxelNoise3D& Noise, int32& ImpurityBudget)
	{
		if (ImpurityBudget <= 0) return;

		// 收集全部 Crack 体素索引
		TArray<int32> CrackIndices;
		CrackIndices.Reserve(Res * Res * Res / 8);
		const int32 Total = Res * Res * Res;
		for (int32 I = 0; I < Total; ++I)
		{
			if (Occ[I] && Data[I] == Crack) CrackIndices.Add(I);
		}
		if (CrackIndices.Num() == 0) return;

		// Fisher-Yates 洗牌避免扫描方向偏差
		for (int32 I = CrackIndices.Num() - 1; I > 0; --I)
		{
			const int32 J = Rng.RandHelper(I + 1);
			Swap(CrackIndices[I], CrackIndices[J]);
		}

		for (int32 CrackIdx : CrackIndices)
		{
			if (ImpurityBudget <= 0) break;

			const int32 CX = CrackIdx % Res;
			const int32 CY = (CrackIdx / Res) % Res;
			const int32 CZ = CrackIdx / (Res * Res);

			// 26-邻域
			for (int32 DZ = -1; DZ <= 1 && ImpurityBudget > 0; ++DZ)
			for (int32 DY = -1; DY <= 1 && ImpurityBudget > 0; ++DY)
			for (int32 DX = -1; DX <= 1 && ImpurityBudget > 0; ++DX)
			{
				if (DX == 0 && DY == 0 && DZ == 0) continue;
				const int32 NX = CX + DX, NY = CY + DY, NZ = CZ + DZ;
				if (NX < 0 || NX >= Res || NY < 0 || NY >= Res || NZ < 0 || NZ >= Res) continue;
				const int32 NIdx = (NZ * Res + NY) * Res + NX;
				if (!Occ[NIdx] || Data[NIdx] != JadeBody) continue;

				// 用高频率噪声判定：> 0.15 决定浸染（约 85% 通过率）
				if (Noise.Noise(NX * 0.3f, NY * 0.3f, NZ * 0.3f) > 0.15f)
				{
					Data[NIdx] = Impurity;
					--ImpurityBudget;
				}
			}
		}
	}

	/** 第三步：皮壳渗透杂质兜底。裂纹太少时，用 mesh 边缘的 JadeBody 转为 Impurity。
	 *  模拟天然风化皮渗透（铁锈浸染）。 */
	void GenerateSkinInfiltration(TArray<uint8>& Data, const TArray<uint8>& Occ, int32 Res,
		FRandomStream& Rng, const FVoxelNoise3D& Noise, int32& ImpurityBudget)
	{
		if (ImpurityBudget <= 0) return;

		// 收集边缘 JadeBody 体素（6-邻域中有外部）
		TArray<int32> EdgeJadeIndices;
		EdgeJadeIndices.Reserve(Res * Res * Res / 8);
		const int32 Total = Res * Res * Res;
		for (int32 I = 0; I < Total; ++I)
		{
			if (!Occ[I] || Data[I] != JadeBody) continue;
			const int32 X = I % Res;
			const int32 Y = (I / Res) % Res;
			const int32 Z = I / (Res * Res);
			bool bEdge = false;
			const int32 Nbrs[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
			for (const auto& D : Nbrs)
			{
				const int32 NX = X + D[0], NY = Y + D[1], NZ = Z + D[2];
				if (NX < 0 || NX >= Res || NY < 0 || NY >= Res || NZ < 0 || NZ >= Res) { bEdge = true; break; }
				if (!Occ[(NZ * Res + NY) * Res + NX]) { bEdge = true; break; }
			}
			if (bEdge) EdgeJadeIndices.Add(I);
		}
		if (EdgeJadeIndices.Num() == 0) return;

		// 洗牌
		for (int32 I = EdgeJadeIndices.Num() - 1; I > 0; --I)
		{
			const int32 J = Rng.RandHelper(I + 1);
			Swap(EdgeJadeIndices[I], EdgeJadeIndices[J]);
		}

		for (int32 EdgeIdx : EdgeJadeIndices)
		{
			if (ImpurityBudget <= 0) break;
			const int32 X = EdgeIdx % Res;
			const int32 Y = (EdgeIdx / Res) % Res;
			const int32 Z = EdgeIdx / (Res * Res);
			if (Noise.Noise(X * 0.25f, Y * 0.25f, Z * 0.25f) > 0.4f)
			{
				Data[EdgeIdx] = Impurity;
				--ImpurityBudget;
			}
		}
	}

	// ---- mesh 体素化 ----

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

	// 3. 三步法生成缺陷：噪声断层裂纹 + 沿裂浸染杂质 + 皮壳渗透兜底
	{
		FRandomStream Rng(VoxelMakeSubSeed(Seed, 0));

		// 统计占用体素数（仅 OccupancyMask==1）
		int32 TotalOccupied = 0;
		for (int32 I = 0; I < Total; ++I) { if (Result.OccupancyMask[I]) ++TotalOccupied; }

		const int32 TargetVoxels = FMath::RoundToInt(
			static_cast<float>(TotalOccupied) * FMath::Clamp(TargetCoverage, 0.05f, 0.85f));
		// 裂纹 60% / 杂质 40%
		int32 CrackBudget = FMath::RoundToInt(static_cast<float>(TargetVoxels) * 0.6f);
		int32 ImpurityBudget = FMath::RoundToInt(static_cast<float>(TargetVoxels) * 0.4f);

		FVoxelNoise3D Noise;
		Noise.Init(VoxelMakeSubSeed(Seed, 100));


		GeneratePlanesOrStrands(Result.Data, Result.OccupancyMask, Res, Rng, Noise, CrackBudget);

		GenerateImpurityDiffusion(Result.Data, Result.OccupancyMask, Res, Rng, Noise, ImpurityBudget);

		if (ImpurityBudget > 0)
		{
			GenerateSkinInfiltration(Result.Data, Result.OccupancyMask, Res, Rng, Noise, ImpurityBudget);
		}
	}

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

uint8 FClcStoneVoxelField3D::SampleNearestAtLocalPos(const FVector& LocalPos) const
{
	int32 X, Y, Z;
	LocalToVoxelInt(LocalPos, X, Y, Z);
	if (X < 0 || Y < 0 || Z < 0) return 0;
	const int32 Idx = IndexOf(X, Y, Z);
	return Idx >= 0 ? Data[Idx] : 0;
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
