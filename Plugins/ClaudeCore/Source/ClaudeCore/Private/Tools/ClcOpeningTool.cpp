// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcOpeningTool.h"
#include "Actors/ClcOpeningStone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcOpeningMaskComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"

AClcOpeningTool::AClcOpeningTool()
{
	MaxDurability = 200.0f;
	CurrentDurability = MaxDurability;

	// 开窗器位姿平滑提速——命中点是即时的，工具跟太慢会不匹配，调快让工具更跟手
	SmoothSpeedLocation = 0.3f;
	SmoothSpeedRotation = 0.35f;
}

void AClcOpeningTool::OnLeftClick(bool bPressed)
{
	bIsGrinding = bPressed && !IsBroken();
}

void AClcOpeningTool::OnUpdate(const FClcToolTraceInfo& TraceInfo)
{
	if (!TargetStone) return;

	// ─── 工具定位：贴在命中点，Z 轴对齐法线 ───
	if (TraceInfo.bHasHit)
	{
		const FVector SafeNormal = TraceInfo.SurfaceNormal.GetSafeNormal();
		TargetLocation = TraceInfo.HitPoint;
		TargetRotation = FQuat::FindBetween(FVector::UpVector, SafeNormal).Rotator();
		bHasTarget = true;
	}
	else
	{
		bHasTarget = false;
	}

	// ─── 打磨中 → 执行逻辑 ───
	if (bIsGrinding && !IsBroken() && TraceInfo.bHasHit)
	{
		ExecuteGrind(TraceInfo.RayOrigin, TraceInfo.RayDirection);
		ConsumeDurability(DurabilityPerSecond * GetWorld()->GetDeltaSeconds());
	}
}

bool AClcOpeningTool::ExecuteGrind(const FVector& RayOrigin, const FVector& RayDirection)
{
	if (!TargetStone) return false;

	UStaticMeshComponent* StoneMeshComp = TargetStone->GetStoneMesh();
	if (!StoneMeshComp) return false;

	// ─── 手动射线-三角形求交（Chaos 不返回 FaceIndex，遍历 LOD0 全部三角形）───
	const FTransform& MeshTM = StoneMeshComp->GetComponentToWorld();
	const FVector LocalRayOrigin = MeshTM.InverseTransformPosition(RayOrigin);
	const FVector LocalRayDir    = MeshTM.InverseTransformVectorNoScale(RayDirection);

	UStaticMesh* Mesh = StoneMeshComp->GetStaticMesh();
	if (!Mesh) return false;
	FStaticMeshRenderData* RD = Mesh->GetRenderData();
	if (!RD || RD->LODResources.Num() == 0) return false;
	const FStaticMeshLODResources& LOD = RD->LODResources[0];
	const FPositionVertexBuffer&   PosVB = LOD.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& SMVB  = LOD.VertexBuffers.StaticMeshVertexBuffer;
	const uint32 NumTriangles = LOD.IndexBuffer.GetNumIndices() / 3;

	// Möller–Trumbore 逐三角形求交，取最近命中
	constexpr float EPS = 1e-6f;
	float   BestT = 1e30f;
	int32   BestTri = -1;
	float   BestU = 0.f, BestV = 0.f;

	for (uint32 Tri = 0; Tri < NumTriangles; ++Tri)
	{
		const uint32 I0 = LOD.IndexBuffer.GetIndex(Tri * 3);
		const uint32 I1 = LOD.IndexBuffer.GetIndex(Tri * 3 + 1);
		const uint32 I2 = LOD.IndexBuffer.GetIndex(Tri * 3 + 2);

		const FVector V0 = (FVector)PosVB.VertexPosition(I0);
		const FVector V1 = (FVector)PosVB.VertexPosition(I1);
		const FVector V2 = (FVector)PosVB.VertexPosition(I2);

		const FVector E1 = V1 - V0;
		const FVector E2 = V2 - V0;
		const FVector  H = FVector::CrossProduct(LocalRayDir, E2);
		const float    A = FVector::DotProduct(E1, H);
		if (A > -EPS && A < EPS) continue;
		const float    Fc = 1.0f / A;
		const FVector  S  = LocalRayOrigin - V0;
		const float    U  = Fc * FVector::DotProduct(S, H);
		if (U < 0.0f || U > 1.0f) continue;
		const FVector  Qv = FVector::CrossProduct(S, E1);
		const float    V  = Fc * FVector::DotProduct(LocalRayDir, Qv);
		if (V < 0.0f || U + V > 1.0f) continue;
		const float    T  = Fc * FVector::DotProduct(E2, Qv);
		if (T > EPS && T < BestT)
		{
			BestT = T;  BestTri = (int32)Tri;  BestU = U;  BestV = V;
		}
	}

	if (BestTri < 0) return false;

	// 命中三角形 → UV 插值
	const uint32 BI0 = LOD.IndexBuffer.GetIndex(BestTri * 3);
	const uint32 BI1 = LOD.IndexBuffer.GetIndex(BestTri * 3 + 1);
	const uint32 BI2 = LOD.IndexBuffer.GetIndex(BestTri * 3 + 2);
	const FVector2D UV0 = (FVector2D)SMVB.GetVertexUV(BI0, 0);
	const FVector2D UV1 = (FVector2D)SMVB.GetVertexUV(BI1, 0);
	const FVector2D UV2 = (FVector2D)SMVB.GetVertexUV(BI2, 0);

	const float Bu = 1.0f - BestU - BestV;
	const FVector2D HitUV = Bu * UV0 + BestU * UV1 + BestV * UV2;

	// 委托 Stone 执行打磨 + 更新统计
	return TargetStone->GrindAtUV(HitUV.X, HitUV.Y);
}
