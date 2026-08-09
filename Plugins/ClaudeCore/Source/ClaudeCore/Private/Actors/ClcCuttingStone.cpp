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
#include "Data/ClcJadeTextureConfig.h"
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

	// ---- 4b. 切面材质 MID（PBR 玉杂 lerp；贴图由 DA_JadeTextureConfig 注入，与 M_StoneOpening 同链路） ----
	if (UMaterialInterface* CutFaceMat = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/JadeBetting/Materials/M_StoneCutFace.M_StoneCutFace")))
	{
		CutFaceMID = UMaterialInstanceDynamic::Create(CutFaceMat, this, TEXT("CutFaceMID"));
		if (CutFaceMID)
		{
			if (UClcJadeTextureConfig* JadeCfg = LoadObject<UClcJadeTextureConfig>(
				nullptr, *GetDefault<UClcDeveloperSettings>()->JadeTextureConfigPath))
			{
				JadeCfg->InjectIntoMID(CutFaceMID);
			}
		}
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
	UMaterialInterface* CapMat = CutFaceMID;
	if (!CapMat)
	{
		if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/JadeBetting/Materials/M_StoneCutFace.M_StoneCutFace")))
		{
			CapMat = Loaded;
		}
	}
	if (!CapMat)
	{
		CapMat = (GEngine && GEngine->VertexColorMaterial)
			? static_cast<UMaterialInterface*>(GEngine->VertexColorMaterial)
			: static_cast<UMaterialInterface*>(ShellMID);
	}

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

		// cap 顶点色 + planar UV（按切平面法线投影生成，避免退化 UV）
		const int32 CapIdx = CutMesh->GetNumSections() - 1;
		ApplyVoxelColorsToSection(CapIdx, P.Normal);
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

	UMaterialInterface* CapMat = CutFaceMID;
	if (!CapMat)
	{
		if (UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/JadeBetting/Materials/M_StoneCutFace.M_StoneCutFace")))
		{
			CapMat = Loaded;
		}
	}
	if (!CapMat)
	{
		CapMat = (GEngine && GEngine->VertexColorMaterial)
			? static_cast<UMaterialInterface*>(GEngine->VertexColorMaterial)
			: static_cast<UMaterialInterface*>(ShellMID);
	}

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		CutMesh, PlanePointWorld, SliceNormal,
		true, OtherHalf,
		EProcMeshSliceCapOption::CreateNewSectionForCap, CapMat);

	LastOtherHalf = OtherHalf;

	if (OtherHalf)
	{
		const FString HalfName = OtherHalf->GetName();
		const int32 HalfSections = OtherHalf->GetNumSections();
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcCuttingStone][Cut] OtherHalf=%s sections=%d"), *HalfName, HalfSections);
	}
	else
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcCuttingStone][Cut] OtherHalf=NULL"));
	}

	// ---- 6. cap 顶点色 + planar UV ----
	const int32 CapIdx = CutMesh->GetNumSections() - 1;
	ApplyVoxelColorsToSection(CapIdx, PlaneNormal);

	// ---- 7. 切走块位置缓存（飞金币动效用） ----
	LastCutPieceWorldCenter = CutMesh->GetComponentTransform().TransformPosition(-PlaneDistance * PlaneNormal);

	UE_LOG(LogClaudeCore, Log,
		TEXT("[ClcCuttingStone] Cut d=%.2f removeNeg=%d -> away total=%d jade=%d crack=%d | remain vol=%.1fcm3"),
		PlaneDistance, static_cast<int32>(bActuallyRemoveNeg), OutCutAwayTotal,
		OutCutAwayJade, OutCutAwayCrack, CachedStoneData.RemainingVolume);
	return true;
}

bool AClcCuttingStone::PredictCutSide(const FVector& PlanePointWorld,
	const FVector& PlaneNormalWorld, bool& OutRemoveNegative) const
{
	OutRemoveNegative = false;
	if (!bInitialized) return false;

	FVector PlaneNormal;
	float PlaneDistance = 0.0f;
	if (!BuildLocalCutPlane(PlanePointWorld, PlaneNormalWorld, PlaneNormal, PlaneDistance))
	{
		return false;
	}

	const FClcStoneVoxelField3D::FSliceCounts Counts =
		VoxelField.ComputeSliceCounts(PlaneNormal, PlaneDistance);
	if (Counts.NegTotal <= 0 || Counts.PosTotal <= 0)
	{
		return false;
	}

	// 与 ExecuteCut 的自动切较小侧逻辑一致
	OutRemoveNegative = (Counts.NegTotal <= Counts.PosTotal);
	return true;
}

void AClcCuttingStone::ApplyVoxelColorsToSection(int32 SectionIndex, const FVector& PlaneNormal)
{
	FProcMeshSection* Sec = CutMesh->GetProcMeshSection(SectionIndex);
	if (!Sec) return;

	// 切平面局部法线（已归一化）——用来选 planar 投影的两个轴
	const FVector N = PlaneNormal.GetSafeNormal();
	const FVector AbsN(FMath::Abs(N.X), FMath::Abs(N.Y), FMath::Abs(N.Z));

	// planar 投影缩放：1 单位（cm）= 0.02 UV，约 50cm 周期
	constexpr float UVScale = 0.02f;

	for (FProcMeshVertex& V : Sec->ProcVertexBuffer)
	{
		// 顶点色 G 通道是精确玉肉 mask；其余通道保留调试辨色。
		const FColor Sample = SampleVoxelColor((FVector)V.Position);
		V.Color = FColor(Sample.R, Sample.G, Sample.B, 255);

		// planar UV：按切平面法线的主轴方向选投影面
		// 法线最接近 X → 用 YZ 平面投影；Y → XZ；Z → XY
		FVector2D UV(0.0f, 0.0f);
		if (AbsN.X >= AbsN.Y && AbsN.X >= AbsN.Z)
		{
			UV.Set(V.Position.Y * UVScale, V.Position.Z * UVScale);
		}
		else if (AbsN.Y >= AbsN.X && AbsN.Y >= AbsN.Z)
		{
			UV.Set(V.Position.X * UVScale, V.Position.Z * UVScale);
		}
		else
		{
			UV.Set(V.Position.X * UVScale, V.Position.Y * UVScale);
		}
		V.UV0 = UV;
	}
	CutMesh->SetProcMeshSection(SectionIndex, *Sec);
}

FColor AClcCuttingStone::SampleVoxelColor(const FVector& LocalPos) const
{
	const uint8 V = static_cast<uint8>(FMath::RoundToInt(VoxelField.SampleAtLocalPos(LocalPos)));
	switch (V)
	{
		case JadeBody:  return FColor(40, 255, 90);  // G=255：玉肉 mask
		case Impurity:  return FColor(200, 0, 40);   // G=0：杂质
		case Crack:     return FColor(40, 0, 30);    // G=0：裂纹
		default:        return FColor(120, 0, 120);  // G=0：废肉
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

UPrimitiveComponent* AClcCuttingStone::GetDisplayMesh() const
{
	return CutMesh;
}

void AClcCuttingStone::MarkHaggleResolved(int32 LockedPrice)
{
	CachedStoneData.bHaggleResolved = true;
	CachedStoneData.HaggleLockedPrice = LockedPrice;

	static const FString LockedSuffix = TEXT("【已锁价】");
	static const FString CutSuffix = TEXT("【已解石】");
	if (CachedStoneData.DisplayName.EndsWith(*CutSuffix))
	{
		CachedStoneData.DisplayName.LeftChopInline(CutSuffix.Len());
	}
	if (!CachedStoneData.DisplayName.EndsWith(*LockedSuffix))
	{
		CachedStoneData.DisplayName += LockedSuffix;
	}
}
