// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AClcStoneStall::AClcStoneStall()
{
	PrimaryActorTick.bCanEverTick = false;

	// ── 根组件用无缩放 SceneComponent——避免 StallMesh 缩放同步成 Actor Scale（污染整个 Actor 变换）──
	BenchRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BenchRoot"));
	RootComponent = BenchRoot;

	StallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StallMesh"));
	StallMesh->SetupAttachment(BenchRoot);
	StallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(
		TEXT("/Engine/BasicShapes/Cube"));
	if (DefaultMesh.Succeeded())
	{
		StallMesh->SetStaticMesh(DefaultMesh.Object);
		StallMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
	}

	BallSpawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("BallSpawnPoint"));
	BallSpawnPoint->SetupAttachment(StallMesh);
	BallSpawnPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));

	StoneSpawnCenter = CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnCenter"));
	StoneSpawnCenter->SetupAttachment(StallMesh);
	StoneSpawnCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

	// 预览用 InstancedStaticMesh——挂在 BenchRoot 下，不继承 StallMesh 的桌面缩放
	PreviewGrid = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewGrid"));
	PreviewGrid->SetupAttachment(BenchRoot);
	PreviewGrid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewGrid->SetCastShadow(false);
	PreviewGrid->SetVisibility(false);

	// 用引擎自带的立方体当占位
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PreviewMesh(
		TEXT("/Engine/BasicShapes/Cube"));
	if (PreviewMesh.Succeeded())
	{
		PreviewGrid->SetStaticMesh(PreviewMesh.Object);
	}
}

void AClcStoneStall::BeginPlay()
{
	Super::BeginPlay();

	// 运行时隐藏预览
	PreviewGrid->SetVisibility(false);

	MarketSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UClcStoneMarketSubsystem>();
	if (MarketSubsystem)
	{
		MarketSubsystem->RegisterStall(this);
	}

	SpawnStones();
}

void AClcStoneStall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	RefreshEditorPreview();
#endif
}

bool AClcStoneStall::CalcGridLayout(int32 Count, int32& OutCols, int32& OutRows) const
{
	if (Count <= 0) return false;
	OutCols = FMath::CeilToInt(FMath::Sqrt((float)Count));
	OutRows = FMath::CeilToInt((float)Count / (float)OutCols);
	return true;
}

void AClcStoneStall::RefreshEditorPreview()
{
#if WITH_EDITOR
	// 直接从 DataAsset 读取配置（编辑器里子系统可能未就绪）
	UClcStallConfig* Cfg = LoadObject<UClcStallConfig>(nullptr, TEXT("/Game/JadeBetting/Data/DA_StallConfig"));
	if (!Cfg) return;

	int32 Cols, Rows;
	if (!CalcGridLayout(Cfg->StonesPerStall, Cols, Rows)) return;

	const float CellSize = Cfg->UnitCellSize;
	const float Margin = CellSize * Cfg->MarginCells;
	const float TableWidth = CellSize * Cols + Margin * 2.0f;
	const float TableDepth = CellSize * Rows + Margin * 2.0f;

	// 摊位 Mesh 自适应缩放（含外扩 Margin，表面对齐 StoneSpawnCenter）
	StallMesh->SetRelativeScale3D(FVector(
		TableWidth / 100.0f,
		TableDepth / 100.0f,
		Cfg->StallThicknessScale
	));
	StoneSpawnCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f * Cfg->StallThicknessScale));

	// 格子预览（只画实际的格子区域，预览方块不画到 Margin 里）
	BuildGridPreview(Cols, Rows, CellSize);
#endif
}

void AClcStoneStall::BuildGridPreview(int32 Cols, int32 Rows, float CellSize)
{
#if WITH_EDITOR
	if (!PreviewGrid) return;

	PreviewGrid->ClearInstances();
	PreviewGrid->SetVisibility(true);

	// ── 预览 Mesh 原始尺寸（局部空间）── 用 X 代表 XY（预览 Mesh 一般对称）
	UStaticMesh* PreviewMesh = PreviewGrid->GetStaticMesh();
	float MeshHalfXY = 50.0f;
	float MeshHalfZ  = 50.0f;
	if (PreviewMesh)
	{
		const FBoxSphereBounds& Bounds = PreviewMesh->GetBounds();
		if (Bounds.SphereRadius > 0.0f)
		{
			MeshHalfXY = Bounds.BoxExtent.X;
			MeshHalfZ  = Bounds.BoxExtent.Z;
		}
	}

	// ── PreviewGrid 现挂在 BenchRoot 下，不再继承 StallMesh 的桌面缩放 ──
	// 方案 A 后 Actor Scale 应为 (1,1,1)；下面仍对 ActorScale 做防御性补偿，
	// 即使用户忘记重置旧实例的 Actor Scale，预览方块也能保持正确世界尺寸
	const FVector ActorScale = GetActorScale3D();
	const float ASX = FMath::Abs(ActorScale.X);
	const float ASY = FMath::Abs(ActorScale.Y);
	const float ASZ = FMath::Abs(ActorScale.Z);

	// ── 桌面顶 + 格子中心（统一在 Actor 局部空间算，== PreviewGrid 局部空间）──
	// StallMesh 相对 BenchRoot 的变换已含桌面缩放，用它把 StallMesh/StoneSpawnCenter 的局部点变换上来
	FVector LocalOrigin = FVector::ZeroVector, LocalExtent(50.0f);
	StallMesh->GetLocalBounds(LocalOrigin, LocalExtent);
	const FVector LocalTop(0.0f, 0.0f, LocalOrigin.Z + LocalExtent.Z);
	const FTransform StallRel = StallMesh->GetRelativeTransform();
	const float TableTopZ = StallRel.TransformPosition(LocalTop).Z;
	const FVector SpawnInActor = StallRel.TransformPosition(StoneSpawnCenter->GetRelativeLocation());

	// ── 世界目标尺寸 → 实例局部 Scale（除以 ActorScale，最终世界尺寸回到目标）──
	// 整体缩放到基准的 50%（XY = 格子 40%，Z = 50cm）
	const float TargetXY = CellSize * 0.8f * 0.5f;
	const float TargetZ  = 100.0f * 0.5f;
	const float ScaleXY = TargetXY / FMath::Max(ASX * MeshHalfXY * 2.0f, KINDA_SMALL_NUMBER);
	const float ScaleZ  = TargetZ  / FMath::Max(ASZ * MeshHalfZ  * 2.0f, KINDA_SMALL_NUMBER);
	const FVector Scale(ScaleXY, ScaleXY, ScaleZ);
	const float HalfHeightLocal = MeshHalfZ * ScaleZ;

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			// 跟 SpawnStones 完全一致的偏移公式（围绕中心，间距 CellSize）
			const float OffsetX = (Col - (Cols - 1) * 0.5f) * CellSize;
			const float OffsetY = (Row - (Rows - 1) * 0.5f) * CellSize;
			// 世界偏移 → Actor 局部（除以 ActorScale）
			const float LocalX = SpawnInActor.X + OffsetX / FMath::Max(ASX, KINDA_SMALL_NUMBER);
			const float LocalY = SpawnInActor.Y + OffsetY / FMath::Max(ASY, KINDA_SMALL_NUMBER);

			FTransform InstanceTransform;
			// 底部贴桌面顶：中心 Z = 桌面顶 + 局部半高
			InstanceTransform.SetLocation(FVector(LocalX, LocalY, TableTopZ + HalfHeightLocal));
			InstanceTransform.SetScale3D(Scale);
			InstanceTransform.SetRotation(FQuat::Identity);

			PreviewGrid->AddInstance(InstanceTransform);
		}
	}
#endif
}

void AClcStoneStall::SpawnStones()
{
	for (AClcStone* Stone : SpawnedStones)
	{
		if (Stone) Stone->Destroy();
	}
	SpawnedStones.Empty();

	if (!MarketSubsystem) return;

	UClcStallConfig* StallCfg = MarketSubsystem->GetStallConfig();
	UClcStoneMeshConfig* MeshCfg = MarketSubsystem->GetMeshConfig();
	if (!StallCfg || !MeshCfg) return;

	const int32 Count = StallCfg->StonesPerStall;
	if (Count <= 0) return;

	int32 Cols, Rows;
	if (!CalcGridLayout(Count, Cols, Rows)) return;

	const float CellSize = StallCfg->UnitCellSize;
	const FVector Center = StoneSpawnCenter->GetComponentLocation();
	const float Jitter = CellSize * StallCfg->CellJitterRatio;

	UE_LOG(LogTemp, Log, TEXT("[ClcStall] Grid: %d×%d (CellSize=%.0f)"), Cols, Rows, CellSize);

	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Col = i % Cols;
		const int32 Row = i / Cols;

		const float OffsetX = (Col - (Cols - 1) * 0.5f) * CellSize;
		const float OffsetY = (Row - (Rows - 1) * 0.5f) * CellSize;
		const float JX = FMath::FRandRange(-Jitter, Jitter);
		const float JY = FMath::FRandRange(-Jitter, Jitter);

		bool bSuccess = false;
		FClcStoneInternalData Data = MarketSubsystem->GenerateStoneInternal(bSuccess);
		if (!bSuccess) continue;

		UStaticMesh* Mesh = MeshCfg->GetRandomMesh();
		if (!Mesh) continue;

		// 缩放约束
		const float MeshRadius = Mesh->GetBounds().SphereRadius;
		float Scale = FMath::FRandRange(StallCfg->StoneScaleRange.X, StallCfg->StoneScaleRange.Y);
		if (MeshRadius > 0.0f)
		{
			Scale = FMath::Min(Scale, CellSize / (MeshRadius * 2.0f));
		}

		// 把 Mesh 和 Scale 存入 InternalData，使背包/工作台能沿用同一外观
		Data.StoneMesh = Mesh;
		Data.MeshScale = Scale;

		// Z 轴贴地：石头底部对齐桌面顶面
		// 局部空间石头占据 [Origin.Z - BoxExtent.Z, Origin.Z + BoxExtent.Z]
		// SpawnActor 把局部原点放在 SpawnLoc，要底部贴桌面：
		// SpawnLoc.Z = Center.Z - (Origin.Z - BoxExtent.Z) * Scale
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float BottomOffset = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * Scale;
		const FVector SpawnLoc = Center + FVector(OffsetX + JX, OffsetY + JY, BottomOffset);

		const float Yaw = FMath::FRandRange(0.0f, 360.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		const FString DisplayName = MarketSubsystem->GenerateDisplayName(Data.Origin);

		AClcStone* Stone = GetWorld()->SpawnActor<AClcStone>(AClcStone::StaticClass(), SpawnLoc, FRotator(0, Yaw, 0), Params);
		if (Stone)
		{
			Stone->Initialize(Data, Mesh, Scale, DisplayName);
			SpawnedStones.Add(Stone);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ClcStall] Spawned %d stones"), SpawnedStones.Num());
}

FTransform AClcStoneStall::GetBallSpawnLocation() const
{
	return BallSpawnPoint->GetComponentTransform();
}

float AClcStoneStall::GetTotalTheoreticalValue() const
{
	float Total = 0.0f;
	for (AClcStone* Stone : SpawnedStones)
	{
		if (Stone) Total += Stone->GetStoneData().Internal.TheoreticalValue;
	}
	return Total;
}
