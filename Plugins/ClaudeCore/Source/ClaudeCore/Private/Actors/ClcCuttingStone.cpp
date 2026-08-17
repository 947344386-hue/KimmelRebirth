// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcCuttingStone.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "ClcLog.h"
#include "ClcMeshBufferAccess.h"
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

	// 缓存原石体素统计（尺寸比例 + 预算分摊基准，必须在回放切平面之前——回放会切走体素）
	{
		int32 TotalJade = 0, TotalCrack = 0, TotalImpurity = 0;
		VoxelField.CountRemainingVoxels(TotalVoxels, TotalJade, TotalCrack, TotalImpurity);
		OriginalJade     = TotalJade;
		OriginalCrack    = TotalCrack;
		OriginalImpurity = TotalImpurity;
		if (TotalVoxels <= 0)
		{
			UE_LOG(LogClaudeCore, Warning, TEXT("[ClcCuttingStone] TotalVoxels<=0 after generate!"));
		}
	}

	// 方案1: 3D 体素场独立定价——把原石玉肉总体积/总体积写入 RuntimeData，定价函数不再依赖 2D T
	CachedStoneData.OriginalJadeVolume  = static_cast<float>(OriginalJade)  * VoxelField.VoxelVolume;
	CachedStoneData.OriginalTotalVolume = static_cast<float>(TotalVoxels)   * VoxelField.VoxelVolume;

	// === [CALIB] 上台时同时打印 2D+3D 对照 ===
	{
		const FClcStoneInternalData& D = StoneData.Internal;
		FString GradeStr = TEXT("?");
		if (const UEnum* E = StaticEnum<EClcJadeGrade>())
			GradeStr = E->GetDisplayNameTextByValue(static_cast<int32>(D.Grade)).ToString();
		const float GradeMult =
			D.Grade == EClcJadeGrade::Glass ? 8.0f :
			D.Grade == EClcJadeGrade::Ice ? 4.0f :
			D.Grade == EClcJadeGrade::Glutinous ? 2.0f : 1.0f;
		const float B3D = CachedStoneData.OriginalJadeVolume * 0.30f * GradeMult * 1.3f;
	}

	// ---- 3. 恢复存档：先回放 CutPlanes 到体素场（先体素后 mesh，确保采样正确） ----
	CutPlanes.Reset();
	if (StoneData.CutPlanes.Num() > 0)
	{
		CutPlanes = StoneData.CutPlanes;
		for (const FClcCutPlaneRecord& P : CutPlanes)
		{
			VoxelField.ApplyCut(P.Normal, P.Distance, P.bRemovedNegative);
		}
	}

	// 回放后以体素场为权威重算累计切走/剩余统计
	RefreshCutStatistics();

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
		nullptr, *GetDefault<UClcDeveloperSettings>()->CutFaceMaterialPath))
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
			// 打包版若网格未勾选 Allow CPU Access，索引/顶点数据不驻留 CPU——跳过拷贝，外壳缺失但不崩溃
			const int32 AvailableIndices = ClcGetAvailableIndexCount(LOD.IndexBuffer);
			if (AvailableIndices == 0)
			{
				UE_LOG(LogClaudeCore, Error, TEXT("[ClcCuttingStone] %s 未勾选 Allow CPU Access，打包版无法拷贝网格数据，解石外壳会缺失。"), *GetNameSafe(SourceMesh));
			}
			else
			{
				const FPositionVertexBuffer& PosVB = LOD.VertexBuffers.PositionVertexBuffer;
				const FStaticMeshVertexBuffer& UVB = LOD.VertexBuffers.StaticMeshVertexBuffer;
				const uint32 NumVerts = PosVB.GetNumVertices();
				const uint32 NumIdx = AvailableIndices;

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
			nullptr, *GetDefault<UClcDeveloperSettings>()->CutFaceMaterialPath))
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

		// cap 显示法线保持原有方向；采样方向使用实际保留侧 SliceNormal。
		const int32 CapIdx = CutMesh->GetNumSections() - 1;
		ApplyVoxelColorsToSection(CapIdx, WorldNormal, SliceNormal);
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
	int32& OutCutAwayJade, int32& OutCutAwayCrack, int32& OutCutAwayImpurity)
{
	OutCutAwayTotal = 0;
	OutCutAwayJade = 0;
	OutCutAwayCrack = 0;
	OutCutAwayImpurity = 0;
	LastCutJadeBounds = FBox(ForceInit);
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

	// ---- 4. 体素统计 + 切走侧玉肉包围盒 ----
	const int32 AwayTotal = bActuallyRemoveNeg ? Counts.NegTotal : Counts.PosTotal;
	const int32 AwayJade  = bActuallyRemoveNeg ? Counts.NegJade : Counts.PosJade;
	const int32 AwayCrack = bActuallyRemoveNeg ? Counts.NegCrack : Counts.PosCrack;
	const int32 AwayImpurity = bActuallyRemoveNeg ? Counts.NegImpurity : Counts.PosImpurity;

	// 必须在 ApplyCut 前计算；ApplyCut 会把本刀切走侧全部写入 RemoveMask。
	if (AwayJade > 0)
	{
		const int32 Reso = VoxelField.Resolution;
		FBox& Box = LastCutJadeBounds;
		Box.Init();
		for (int32 X = 0; X < Reso; ++X)
		{
			for (int32 Y = 0; Y < Reso; ++Y)
			{
				for (int32 Z = 0; Z < Reso; ++Z)
				{
					const int32 Idx = VoxelField.IndexOf(X, Y, Z);
					if (Idx < 0 || VoxelField.OccupancyMask[Idx] == 0 || VoxelField.RemoveMask[Idx] != 0)
					{
						continue;
					}
					const FVector VoxelCenter = VoxelField.VoxelToLocal(X, Y, Z);
					const float D = FVector::DotProduct(PlaneNormal, VoxelCenter) + PlaneDistance;
					const bool bInRemovedSide = (D < 0.0f) ? bActuallyRemoveNeg : !bActuallyRemoveNeg;
					if (bInRemovedSide && VoxelField.Data[Idx] == JadeBody)
					{
						Box += VoxelCenter;
					}
				}
			}
		}
	}

	VoxelField.ApplyCut(PlaneNormal, PlaneDistance, bActuallyRemoveNeg);

	FClcCutPlaneRecord Rec;
	Rec.Normal = PlaneNormal;
	Rec.Distance = PlaneDistance;
	Rec.bRemovedNegative = bActuallyRemoveNeg;
	CutPlanes.Add(Rec);

	OutCutAwayTotal    = AwayTotal;
	OutCutAwayJade     = AwayJade;
	OutCutAwayCrack    = AwayCrack;
	OutCutAwayImpurity = AwayImpurity;

	// 以体素场为权威重算全部累计/剩余统计（替代逐刀累加）
	RefreshCutStatistics();
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
			nullptr, *GetDefault<UClcDeveloperSettings>()->CutFaceMaterialPath))
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

	// ---- 6. cap 顶点色 + planar UV（保留块 + 切下块各自向内部采样） ----
	const int32 CapIdx = CutMesh->GetNumSections() - 1;
	ApplyVoxelColorsToSection(CapIdx, PlaneNormalWorld, SliceNormal);

	// 切下块沿 -SliceNormal 采样，显示法线仍保持与保留块相反。
	if (OtherHalf && OtherHalf->GetNumSections() > 0)
	{
		const int32 OtherCapIdx = OtherHalf->GetNumSections() - 1;
		ApplyVoxelColorsToSection(OtherCapIdx, -PlaneNormalWorld, -SliceNormal, OtherHalf);
	}

	// ---- 7. 切走块位置缓存（飞金币动效用） ----
	LastCutPieceWorldCenter = CutMesh->GetComponentTransform().TransformPosition(-PlaneDistance * PlaneNormal);

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

bool AClcCuttingStone::PredictCutRatio(const FVector& PlanePointWorld,
	const FVector& PlaneNormalWorld, float& OutRatio) const
{
	OutRatio = 0.0f;
	if (!bInitialized || TotalVoxels <= 0) return false;

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
		return false; // 刀口未同时穿过两侧，与 CanCutAtWorldPlane 一致
	}

	// 切走侧 = 较小侧（与 ExecuteCut 自动选侧一致）
	const int32 CutAwayTotal = FMath::Min(Counts.NegTotal, Counts.PosTotal);
	OutRatio = static_cast<float>(CutAwayTotal) / static_cast<float>(TotalVoxels);
	return true;
}

void AClcCuttingStone::ApplyVoxelColorsToSection(int32 SectionIndex, const FVector& SurfaceNormalWorld,
	const FVector& InteriorSampleDirectionWorld, UProceduralMeshComponent* TargetMesh)
{
	UProceduralMeshComponent* Mesh = TargetMesh ? TargetMesh : CutMesh;
	if (!Mesh) return;
	FProcMeshSection* Sec = Mesh->GetProcMeshSection(SectionIndex);
	if (!Sec) return;

	const FTransform& CompTransform = Mesh->GetComponentTransform();
	const FVector LocalN = CompTransform.InverseTransformVectorNoScale(SurfaceNormalWorld).GetSafeNormal();
	FVector LocalSampleDirection = CompTransform.InverseTransformVectorNoScale(
		InteriorSampleDirectionWorld).GetSafeNormal();
	if (LocalSampleDirection.IsNearlyZero())
	{
		LocalSampleDirection = LocalN;
	}

	// 统一切线/副法线（与局部法线正交，供材质 planar UV 投影）
	FVector LocalAxisU, LocalAxisV;
	LocalN.FindBestAxisVectors(LocalAxisU, LocalAxisV);
	const FProcMeshTangent UnifiedTangent(LocalAxisU, false);

	// planar 投影缩放：1 单位（cm）= 0.02 UV，约 50cm 周期
	constexpr float UVScale = 0.02f;

	const FVector AbsSampleDirection(
		FMath::Abs(LocalSampleDirection.X),
		FMath::Abs(LocalSampleDirection.Y),
		FMath::Abs(LocalSampleDirection.Z));
	const float HalfVoxelAlongSampleDirection = 0.5f * FVector::DotProduct(
		AbsSampleDirection, VoxelField.VoxelSize);

	for (FProcMeshVertex& V : Sec->ProcVertexBuffer)
	{
		// 统一法线+切线覆盖，消灭三角扇放射状折光
		V.Normal = LocalN;
		V.Tangent = UnifiedTangent;

		const FColor Sample = SampleVoxelColor(
			(FVector)V.Position + LocalSampleDirection * HalfVoxelAlongSampleDirection);
		const uint8 HardG = (Sample.G > 128) ? 255 : 0;
		V.Color = FColor(Sample.R, HardG, Sample.B, 255);

		// planar UV：按局部坐标切面切线/副法线投影
		V.UV0 = FVector2D(
			FVector::DotProduct((FVector)V.Position, LocalAxisU) * UVScale,
			FVector::DotProduct((FVector)V.Position, LocalAxisV) * UVScale);
	}
	Mesh->SetProcMeshSection(SectionIndex, *Sec);
}

FColor AClcCuttingStone::SampleVoxelColor(const FVector& LocalPos) const
{
	const uint8 V = VoxelField.SampleNearestAtLocalPos(LocalPos);
	switch (V)
	{
		case JadeBody:  return FColor(0,   255, 120, 255);  // G高=翡翠绿
		case Impurity:  return FColor(220, 80,  20,  255);  // R高=铁锈褐红
		case Crack:     return FColor(20,  20,  20,  255);  // 全低=暗黑裂纹
		default:        return FColor(100, 100, 100, 255);  // 灰底废肉
	}
}

// ============================================================
// 体素统计刷新（以体素场为权威，初始化回放后/每次 ApplyCut 后调用）
// ============================================================

void AClcCuttingStone::RefreshCutStatistics()
{
	if (TotalVoxels <= 0 || VoxelField.Data.Num() == 0) return;

	const float VoxelVolume = VoxelField.VoxelVolume;
	int32 RemainingTotal = 0, RemainingJade = 0, RemainingCrack = 0, RemainingImpurity = 0;
	VoxelField.CountRemainingVoxels(RemainingTotal, RemainingJade, RemainingCrack, RemainingImpurity);

	// 累计切走 = 原始 − 剩余（以体素场为权威，替代逐刀累加）
	CachedStoneData.RemainingVolume     = static_cast<float>(RemainingTotal) * VoxelVolume;
	CachedStoneData.RemainingJadeVolume = static_cast<float>(RemainingJade) * VoxelVolume;
	CachedStoneData.ExposedCutVolume    = static_cast<float>(TotalVoxels  - RemainingTotal)  * VoxelVolume;
	CachedStoneData.ExposedJadeVolume   = static_cast<float>(OriginalJade - RemainingJade)  * VoxelVolume;
	CachedStoneData.ExposedCrackVolume  = static_cast<float>(OriginalCrack - RemainingCrack) * VoxelVolume;
}

// ============================================================
// 切石结算（由解石台调用，让石头 Actor 自己更新持久字段）
// ============================================================

void AClcCuttingStone::ApplyCutSettlement(int32 ConsumedBudgetAfter, int32 PieceGold)
{
	CachedStoneData.ConsumedCutBudget = ConsumedBudgetAfter;
	CachedStoneData.TotalSettledValue += PieceGold;
}

// ============================================================
// 存档 / 查询
// ============================================================

bool AClcCuttingStone::GetStoneData(FClcStoneRuntimeData& OutData) const
{
	if (!bInitialized) return false;
	OutData = CachedStoneData;
	OutData.CutPlanes = CutPlanes;
	// 补充体素场权威剩余体积（RefreshCutStatistics 已写入 CachedStoneData，这里只是兜底）
	int32 RT = 0, RJ = 0, RC = 0, RI = 0;
	VoxelField.CountRemainingVoxels(RT, RJ, RC, RI);
	OutData.RemainingVolume     = static_cast<float>(RT) * VoxelField.VoxelVolume;
	OutData.RemainingJadeVolume = static_cast<float>(RJ) * VoxelField.VoxelVolume;
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
	// 按实际后缀匹配移除，避免 DisplayName 在【已解石】后还有其他字符时按长度切错位置
	CachedStoneData.DisplayName.RemoveFromEnd(*CutSuffix, ESearchCase::CaseSensitive);
	if (!CachedStoneData.DisplayName.EndsWith(*LockedSuffix))
	{
		CachedStoneData.DisplayName += LockedSuffix;
	}
}
