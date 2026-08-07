// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcCuttingStone.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "ClcLog.h"
#include "Data/ClcJadeTypes.h"
#include "Data/ClcShellTextureConfig.h"
#include "ClcDeveloperSettings.h"

// ============================================================

AClcCuttingStone::AClcCuttingStone()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CutMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CutMesh"));
	CutMesh->SetupAttachment(RootComponent);
	CutMesh->SetCastShadow(true);
	CutMesh->SetMobility(EComponentMobility::Movable);
}

void AClcCuttingStone::BeginPlay()
{
	Super::BeginPlay();
}

// ============================================================
// 初始化
// ============================================================

bool AClcCuttingStone::Initialize(const FClcStoneRuntimeData& StoneData, int32 DefectCount,
	float TargetCoverage, int32 VoxelResolution, const FString& ShellMaterialPath)
{
	CachedStoneData = StoneData;

	// ---- 1. 加载源 mesh ----
	UStaticMesh* Mesh = nullptr;
	if (StoneData.Internal.StoneMesh.IsValid())
	{
		Mesh = StoneData.Internal.StoneMesh.Get();
	}
	else
	{
		Mesh = StoneData.Internal.StoneMesh.LoadSynchronous();
	}
	if (!Mesh)
	{
		UE_LOG(LogClaudeCore, Error, TEXT("[ClcCuttingStone] Failed to load stone mesh!"));
		return false;
	}
	SourceMesh = Mesh;

	const float MeshScale = FMath::Max(FMath::Abs(StoneData.Internal.MeshScale), KINDA_SMALL_NUMBER);
	const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
	CutMesh->SetRelativeScale3D(FVector(MeshScale));
	CutMesh->SetRelativeLocation(-MeshBounds.Origin * MeshScale);

	// ---- 2. 生成 3D 体素场 ----
	VoxelField = FClcStoneVoxelField3D::Generate(
		StoneData.Internal.Seed, Mesh, VoxelResolution, DefectCount, TargetCoverage);
	VoxelField.VoxelVolume *= MeshScale * MeshScale * MeshScale;

	// ---- 3. 恢复存档：先回放 CutPlanes 到体素场（先体素后 mesh，确保采样正确） ----
	CutPlanes.Reset();
	if (StoneData.CutPlanes.Num() > 0)
	{
		CutPlanes = StoneData.CutPlanes;
		for (const FClcCutPlaneRecord& P : CutPlanes)
		{
			VoxelField.ApplyCut(P.Normal, P.Distance, P.bRemovedNegative);
		}
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcCuttingStone] Restored %d cut planes."), CutPlanes.Num());
	}

	// ---- 4. 外壳材质 ----
	UMaterialInterface* ShellMat = LoadObject<UMaterialInterface>(nullptr, *ShellMaterialPath);
	if (ShellMat)
	{
		ShellMID = CutMesh->CreateDynamicMaterialInstance(0, ShellMat, TEXT("ShellMID"));
		if (ShellMID)
		{
			UClcShellTextureConfig* ShellCfg = LoadObject<UClcShellTextureConfig>(
				nullptr, *GetDefault<UClcDeveloperSettings>()->ShellTextureConfigPath);
			if (ShellCfg)
			{
				ShellCfg->InjectTexturesIntoMID(ShellMID, StoneData.Internal.ShellTypeIndex);
			}
		}
	}
	else
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcCuttingStone] Shell material not found: %s"), *ShellMaterialPath);
	}

	// ---- 5. 从源 mesh LOD0 拷贝到 PMC section 0（体素场生成用的同一套渲染数据 API） ----
	CutMesh->ClearAllMeshSections();
	{
		const FStaticMeshRenderData* RenderData = SourceMesh->GetRenderData();
		if (RenderData && RenderData->LODResources.Num() > 0)
		{
			const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
			const FPositionVertexBuffer& PosVB = LOD.VertexBuffers.PositionVertexBuffer;
			const FStaticMeshVertexBuffer& UVB = LOD.VertexBuffers.StaticMeshVertexBuffer;
			const uint32 NumVerts = PosVB.GetNumVertices();
			const uint32 NumIdx = LOD.IndexBuffer.GetNumIndices();

			TArray<FVector> ShellVerts;
			TArray<int32> ShellTris;
			TArray<FVector2D> ShellUVs;
			TArray<FVector> ShellNormals;
			TArray<FProcMeshTangent> ShellTangents;

			ShellVerts.Reserve(NumVerts);
			ShellUVs.Reserve(NumVerts);
			ShellNormals.Reserve(NumVerts);
			ShellTangents.Reserve(NumVerts);
			for (uint32 I = 0; I < NumVerts; ++I)
			{
				ShellVerts.Add(FVector(PosVB.VertexPosition(I)));
				ShellUVs.Add(FVector2D(UVB.GetVertexUV(I, 0)));
				ShellNormals.Add(FVector(UVB.VertexTangentZ(I)));
				FProcMeshTangent T;
				T.TangentX = FVector(UVB.VertexTangentX(I));
				T.bFlipTangentY = false;
				ShellTangents.Add(T);
			}
			ShellTris.Reserve(NumIdx);
			for (uint32 I = 0; I < NumIdx; ++I)
			{
				ShellTris.Add(LOD.IndexBuffer.GetIndex(I));
			}
			CutMesh->CreateMeshSection(0, ShellVerts, ShellTris, ShellNormals, ShellUVs,
				TArray<FColor>(), ShellTangents, false);
		}
	}
	if (ShellMID)
	{
		CutMesh->SetMaterial(0, ShellMID);
	}

	// ---- 6. 用引擎 SliceProceduralMesh 回放切平面到 PMC ----
	ReplayAllCuts();

	bInitialized = true;
	return true;
}

// ============================================================
// 回放全部切平面（初始化恢复 / 后续可用）
// ============================================================

void AClcCuttingStone::ReplayAllCuts()
{
	UMaterialInterface* CapMat = (GEngine && GEngine->VertexColorMaterial)
		? static_cast<UMaterialInterface*>(GEngine->VertexColorMaterial)
		: static_cast<UMaterialInterface*>(ShellMID);

	const FTransform PMCToWorld = CutMesh->GetComponentTransform();

	for (const FClcCutPlaneRecord& P : CutPlanes)
	{
		const FVector LocalPoint = -P.Distance * P.Normal;
		const FVector WorldPoint = PMCToWorld.TransformPosition(LocalPoint);
		const FVector WorldNormal = PMCToWorld.TransformVectorNoScale(P.Normal).GetSafeNormal();
		// SliceProceduralMesh 保留平面正侧（沿法线方向）。
		// bRemovedNegative=true → 去负留正 → 法线不变
		// bRemovedNegative=false → 去正留负 → 法线反向
		const FVector SliceNormal = P.bRemovedNegative ? WorldNormal : -WorldNormal;

		UProceduralMeshComponent* OtherHalf = nullptr;
		UKismetProceduralMeshLibrary::SliceProceduralMesh(
			CutMesh, WorldPoint, SliceNormal,
			false, OtherHalf,
			EProcMeshSliceCapOption::CreateNewSectionForCap, CapMat);

		// 顶点色：用体素场在 cap 顶点位置采样写入
		const int32 CapIdx = CutMesh->GetNumSections() - 1;
		ApplyVoxelColorsToSection(CapIdx);
	}
}

// ============================================================
// 切平面坐标转换
// ============================================================

bool AClcCuttingStone::BuildLocalCutPlane(const FVector& PlanePointWorld,
	const FVector& PlaneNormalWorld, FVector& OutPlaneNormal, float& OutPlaneDistance) const
{
	if (!CutMesh || PlaneNormalWorld.IsNearlyZero()) return false;

	const FTransform MeshTransform = CutMesh->GetComponentTransform();
	OutPlaneNormal = MeshTransform.InverseTransformVectorNoScale(PlaneNormalWorld).GetSafeNormal();
	if (OutPlaneNormal.IsNearlyZero()) return false;

	const FVector LocalPoint = MeshTransform.InverseTransformPosition(PlanePointWorld);
	OutPlaneDistance = -FVector::DotProduct(OutPlaneNormal, LocalPoint);
	return true;
}

bool AClcCuttingStone::CanCutAtWorldPlane(const FVector& PlanePointWorld,
	const FVector& PlaneNormalWorld) const
{
	if (!bInitialized) return false;

	FVector PlaneNormal;
	float PlaneDistance = 0.0f;
	if (!BuildLocalCutPlane(PlanePointWorld, PlaneNormalWorld, PlaneNormal, PlaneDistance))
	{
		return false;
	}

	const FClcStoneVoxelField3D::FSliceCounts Counts =
		VoxelField.ComputeSliceCounts(PlaneNormal, PlaneDistance);
	return Counts.NegTotal > 0 && Counts.PosTotal > 0;
}

// ============================================================
// 解石
// ============================================================

bool AClcCuttingStone::ExecuteCut(const FVector& PlanePointWorld, const FVector& PlaneNormalWorld,
	bool bForceRemoveNegativeSide, int32& OutCutAwayTotal,
	int32& OutCutAwayJade, int32& OutCutAwayCrack)
{
	OutCutAwayTotal = 0;
	OutCutAwayJade = 0;
	OutCutAwayCrack = 0;
	if (!bInitialized || !SourceMesh) return false;

	// ---- 1. 局部空间切平面 ----
	FVector PlaneNormal;
	float PlaneDistance = 0.0f;
	if (!BuildLocalCutPlane(PlanePointWorld, PlaneNormalWorld, PlaneNormal, PlaneDistance))
	{
		return false;
	}

	// ---- 2. 体素场两侧计数 ----
	const FClcStoneVoxelField3D::FSliceCounts Counts =
		VoxelField.ComputeSliceCounts(PlaneNormal, PlaneDistance);
	if (Counts.NegTotal <= 0 || Counts.PosTotal <= 0)
	{
		return false;
	}

	// ---- 3. 自动切走较小侧 ----
	const bool bNegSmaller = Counts.NegTotal <= Counts.PosTotal;
	const bool bActuallyRemoveNeg = bForceRemoveNegativeSide ? true : bNegSmaller;
	VoxelField.ApplyCut(PlaneNormal, PlaneDistance, bActuallyRemoveNeg);

	FClcCutPlaneRecord Rec;
	Rec.Normal = PlaneNormal;
	Rec.Distance = PlaneDistance;
	Rec.bRemovedNegative = bActuallyRemoveNeg;
	CutPlanes.Add(Rec);

	// ---- 4. 体素统计 ----
	if (bActuallyRemoveNeg)
	{
		OutCutAwayTotal = Counts.NegTotal;
		OutCutAwayJade = Counts.NegJade;
		OutCutAwayCrack = Counts.NegCrack;
	}
	else
	{
		OutCutAwayTotal = Counts.PosTotal;
		OutCutAwayJade = Counts.PosJade;
		OutCutAwayCrack = Counts.PosCrack;
	}

	const float VoxelVolume = VoxelField.VoxelVolume;
	CachedStoneData.ExposedCutVolume += static_cast<float>(OutCutAwayTotal) * VoxelVolume;
	CachedStoneData.ExposedJadeVolume += static_cast<float>(OutCutAwayJade) * VoxelVolume;
	CachedStoneData.ExposedCrackVolume += static_cast<float>(OutCutAwayCrack) * VoxelVolume;
	int32 RemainingTotal = 0, RemainingJade = 0, RemainingCrack = 0, RemainingImpurity = 0;
	VoxelField.CountRemainingVoxels(RemainingTotal, RemainingJade, RemainingCrack, RemainingImpurity);
	CachedStoneData.RemainingVolume = static_cast<float>(RemainingTotal) * VoxelVolume;
	if (CachedStoneData.Phase == EClcStonePhase::Unworked)
	{
		CachedStoneData.Phase = EClcStonePhase::Cut;
		static const FString Suffix = TEXT("【已解石】");
		if (!CachedStoneData.DisplayName.EndsWith(*Suffix))
		{
			CachedStoneData.DisplayName += Suffix;
		}
	}

	// ---- 5. 引擎 SliceProceduralMesh（替代手动三角形裁剪 + cap 生成） ----
	// SliceProceduralMesh 保留平面正侧（沿法线方向）。
	// bActuallyRemoveNeg=true → 保留正侧 → 法线不变
	// bActuallyRemoveNeg=false → 保留负侧 → 法线反向
	const FVector SliceNormal = bActuallyRemoveNeg ? PlaneNormalWorld : -PlaneNormalWorld;

	UMaterialInterface* CapMat = (GEngine && GEngine->VertexColorMaterial)
		? static_cast<UMaterialInterface*>(GEngine->VertexColorMaterial)
		: static_cast<UMaterialInterface*>(ShellMID);

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		CutMesh, PlanePointWorld, SliceNormal,
		false, OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap, CapMat);

	// ---- 6. cap 顶点色：体素场采样写入 ----
	const int32 CapIdx = CutMesh->GetNumSections() - 1;
	ApplyVoxelColorsToSection(CapIdx);

	// ---- 7. 切走块位置缓存（飞金币动效用） ----
	LastCutPieceWorldCenter = CutMesh->GetComponentTransform().TransformPosition(-PlaneDistance * PlaneNormal);

	UE_LOG(LogClaudeCore, Log,
		TEXT("[ClcCuttingStone] Cut d=%.2f removeNeg=%d -> away total=%d jade=%d crack=%d | remain vol=%.1fcm3"),
		PlaneDistance, static_cast<int32>(bActuallyRemoveNeg), OutCutAwayTotal,
		OutCutAwayJade, OutCutAwayCrack, CachedStoneData.RemainingVolume);
	return true;
}

// ============================================================
// Cap 顶点色
// ============================================================

void AClcCuttingStone::ApplyVoxelColorsToSection(int32 SectionIndex)
{
	FProcMeshSection* Sec = CutMesh->GetProcMeshSection(SectionIndex);
	if (!Sec) return;

	for (FProcMeshVertex& V : Sec->ProcVertexBuffer)
	{
		V.Color = SampleVoxelColor((FVector)V.Position);
	}
	CutMesh->SetProcMeshSection(SectionIndex, *Sec);
}

FColor AClcCuttingStone::SampleVoxelColor(const FVector& LocalPos) const
{
	const uint8 V = static_cast<uint8>(FMath::RoundToInt(VoxelField.SampleAtLocalPos(LocalPos)));
	switch (V)
	{
		case JadeBody:  return FColor(40, 180, 90);   // 玉=绿
		case Impurity:  return FColor(200, 180, 40);  // 杂=黄
		case Crack:     return FColor(40, 30, 30);    // 裂=暗红黑
		default:        return FColor(120, 120, 120); // 废肉=灰
	}
}

// ============================================================
// 存档 / 查询
// ============================================================

bool AClcCuttingStone::GetStoneData(FClcStoneRuntimeData& OutData) const
{
	if (!bInitialized) return false;
	OutData = CachedStoneData;
	OutData.CutPlanes = CutPlanes;
	int32 RT = 0, RJ = 0, RC = 0, RI = 0;
	VoxelField.CountRemainingVoxels(RT, RJ, RC, RI);
	OutData.RemainingVolume = static_cast<float>(RT) * VoxelField.VoxelVolume;
	return true;
}

void AClcCuttingStone::AutoAlignLongestAxis()
{
	if (!SourceMesh) return;

	const FVector Extent = SourceMesh->GetBounds().BoxExtent;
	FVector LongestAxis = FVector::ForwardVector;
	if (Extent.Y >= Extent.X && Extent.Y >= Extent.Z)
	{
		LongestAxis = FVector::RightVector;
	}
	else if (Extent.Z >= Extent.X && Extent.Z >= Extent.Y)
	{
		LongestAxis = FVector::UpVector;
	}

	const FQuat BaseRotation = GetActorQuat();
	const FVector CurrentWorldAxis = BaseRotation.RotateVector(LongestAxis).GetSafeNormal();
	const FVector TargetWorldAxis = BaseRotation.GetForwardVector().GetSafeNormal();
	const FQuat Alignment = FQuat::FindBetweenNormals(CurrentWorldAxis, TargetWorldAxis);
	SetActorRotation((Alignment * BaseRotation).Rotator());
}

float AClcCuttingStone::GetHalfExtentAlongWorldAxis(const FVector& WorldAxis) const
{
	if (!SourceMesh || !CutMesh || WorldAxis.IsNearlyZero()) return 0.0f;

	const FVector Axis = WorldAxis.GetSafeNormal();
	const FVector Extent = SourceMesh->GetBounds().BoxExtent;
	const FTransform MeshTransform = CutMesh->GetComponentTransform();
	const FVector XExtent = MeshTransform.TransformVector(FVector(Extent.X, 0.0f, 0.0f));
	const FVector YExtent = MeshTransform.TransformVector(FVector(0.0f, Extent.Y, 0.0f));
	const FVector ZExtent = MeshTransform.TransformVector(FVector(0.0f, 0.0f, Extent.Z));
	return FMath::Abs(FVector::DotProduct(Axis, XExtent))
		+ FMath::Abs(FVector::DotProduct(Axis, YExtent))
		+ FMath::Abs(FVector::DotProduct(Axis, ZExtent));
}

FVector AClcCuttingStone::GetCutPieceWorldLocation() const
{
	return LastCutPieceWorldCenter;
}
