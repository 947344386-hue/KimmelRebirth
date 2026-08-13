// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcStoneStall.h"
#include "Actors/ClcStone.h"
#include "Actors/ClcMerchant.h"
#include "Subsystems/ClcStoneMarketSubsystem.h"
#include "Data/ClcStallConfig.h"
#include "Data/ClcStoneMeshConfig.h"
#include "Data/ClcSessionTypes.h"
#include "ClcGameInstance.h"
#include "ClcDeveloperSettings.h"
#include "ClcLog.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ArrowComponent.h"
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

	StoneSpawnCenter = CreateDefaultSubobject<USceneComponent>(TEXT("StoneSpawnCenter"));
	StoneSpawnCenter->SetupAttachment(StallMesh);
	StoneSpawnCenter->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

	// 商人站位锚点——箭头位置=商人站位，箭头朝向=商人面朝方向；每个实例可在蓝图里独立编辑
	MerchantSpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("MerchantSpawnPoint"));
	MerchantSpawnPoint->SetupAttachment(BenchRoot);
	MerchantSpawnPoint->SetRelativeLocation(FVector(150.0f, 0.0f, 0.0f));
	MerchantSpawnPoint->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));

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

	// OnConstruction 的 BuildGridPreview 会把 PreviewGrid 设为可见（编辑器预览），
	// 运行时必须在此显式隐藏，否则预览 cube 会与石头重叠显示
	if (PreviewGrid)
	{
		PreviewGrid->SetVisibility(false);
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			MarketSubsystem = GI->GetSubsystem<UClcStoneMarketSubsystem>();
		}
	}
	if (MarketSubsystem)
	{
		MarketSubsystem->RegisterStall(this);
	}

	SpawnMerchant();

	// 读档恢复：若 GameInstance 缓存了摊位存档数据，跳过随机生成，
	// 等 HandlePostLoadMap 的 next-tick 调 RestoreFromSlots 按 StallId 匹配恢复。
	// 这样做的好处：
	//  (1) 避免随机批次生成后又被打覆盖的视觉闪现；
	//  (2) 即使 RestoreFromSlots 因 MarketSubsystem/StallCfg 未就绪而 early return，
	//      也不会残留一批玩家不该拿到的随机石头。
	UClcGameInstance* ClcGI = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			ClcGI = Cast<UClcGameInstance>(GI);
		}
	}
	const bool bHasPendingRestore = ClcGI && ClcGI->bHasCachedSavedStalls;
	if (bHasPendingRestore)
	{
		UE_LOG(LogClaudeCore, Log, TEXT("[ClcStoneStall] BeginPlay —— 检测到缓存的摊位存档，跳过 SpawnStones，等待 RestoreFromSlots"));
	}
	else
	{
		// 新游戏或无存档：正常生成随机批次
		SpawnStones();
	}
}

void AClcStoneStall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 销毁生成的石头和商人，防止摊位销毁后残留浮空 Actor
	for (AClcStone* Stone : SpawnedStones)
	{
		if (IsValid(Stone))
		{
			Stone->Destroy();
		}
	}
	SpawnedStones.Empty();

	if (IsValid(SpawnedMerchant))
	{
		SpawnedMerchant->Destroy();
		SpawnedMerchant = nullptr;
	}

	if (MarketSubsystem)
	{
		MarketSubsystem->UnregisterStall(this);
	}

	Super::EndPlay(EndPlayReason);
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
	// 直接从 DataAsset 读取配置（编辑器里子系统可能未就绪）——路径走 DeveloperSettings
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	UClcStallConfig* Cfg = LoadObject<UClcStallConfig>(nullptr, *DS->StallConfigPath);
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

	// 新一批石头：清空历史售价基线
	SumBoughtStoneValues = 0.0f;
	BoughtStoneCount = 0;

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
	const FQuat SpawnQuat = StoneSpawnCenter->GetComponentQuat();
	const float Jitter = CellSize * StallCfg->CellJitterRatio;

	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Col = i % Cols;
		const int32 Row = i / Cols;

		const float OffsetX = (Col - (Cols - 1) * 0.5f) * CellSize;
		const float OffsetY = (Row - (Rows - 1) * 0.5f) * CellSize;
		const float JX = FMath::FRandRange(-Jitter, Jitter);
		const float JY = FMath::FRandRange(-Jitter, Jitter);

		bool bSuccess = false;
		FClcStoneInternalData Data = MarketSubsystem->GenerateStoneInternal(bSuccess, SpawnedMerchant ? SpawnedMerchant->GetDeceptionLevel() : 0.5f);
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

		// Z 轴贴地：石头底部对齐桌面顶面（与 AClcStone::SnapToSurface 一致）
		// 局部空间石头顶点在 [Origin - BoxExtent, Origin + BoxExtent]
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float HalfHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
		// 本地网格偏移（含抖动 + Z 贴地）经摊位旋转变换到世界空间——摊位旋转后石头跟着转
		const FVector SpawnLoc = Center + SpawnQuat.RotateVector(FVector(OffsetX + JX, OffsetY + JY, HalfHeight * Scale));

		const float Yaw = FMath::FRandRange(0.0f, 360.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AClcStone* Stone = GetWorld()->SpawnActor<AClcStone>(AClcStone::StaticClass(), SpawnLoc, FRotator(0, Yaw, 0), Params);
		if (Stone)
		{
			Stone->Initialize(Data, Mesh, Scale, FString());
			Stone->SetDisplayName(MarketSubsystem->GenerateDisplayName(Stone->GetStoneData().Internal));
			Stone->SetOwningStall(this);
			Stone->ApplyInteractionConfig(StallCfg->StoneInteractionRadius, StallCfg->StoneAimSweepRadius);
			SpawnedStones.Add(Stone);
		}
	}

	// 石头铺好后补算档位（SpawnMerchant 时空摊会被误判成 Bad）
	if (SpawnedMerchant)
	{
		SpawnedMerchant->RecomputeTier();
	}

	// 生成完成后立即封顶价格（不再依赖 StallZoneManager Tick 轮询）
	ClampAllStones();
}

FVector AClcStoneStall::GetStoneSpawnCenterLocation() const
{
	return StoneSpawnCenter ? StoneSpawnCenter->GetComponentLocation() : GetActorLocation();
}

FName AClcStoneStall::GetStallId() const
{
	// GetPathName() 含完整包路径，PIE 会给关卡加 UEDPIE_<N>_ 前缀，打包则没有。
	// 直接用会导致 PIE 存的档在打包游戏里读不出（StallId 不匹配）。
	// 剥掉 UEDPIE_<N>_ 前缀，使同一摊位在 PIE 和打包环境下 Id 一致。
	FString PathName = GetPathName();
	static const FString PiePrefix = TEXT("UEDPIE_");
	if (PathName.StartsWith(PiePrefix))
	{
		// 跳过 "UEDPIE_"（7 字符），再跳过数字和随后的 '_'，剩 Map_...:PersistentLevel.xxx
		int32 Idx = PiePrefix.Len();
		while (Idx < PathName.Len() && FChar::IsDigit(PathName[Idx])) { ++Idx; }
		if (Idx < PathName.Len() && PathName[Idx] == '_') { ++Idx; }
		PathName = PathName.RightChop(Idx);
	}
	return FName(*PathName);
}

float AClcStoneStall::GetTotalTheoreticalValue() const
{
	float Total = 0.0f;
	for (AClcStone* Stone : SpawnedStones)
	{
		if (IsValid(Stone)) Total += Stone->GetStoneData().Internal.TheoreticalValue;
	}
	return Total;
}

void AClcStoneStall::SpawnMerchant()
{
	if (SpawnedMerchant) return;
	if (!MerchantSpawnPoint) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 从 MerchantSpawnPoint 箭头取位置和朝向——箭头位置=商人站位，箭头朝向=商人面朝方向
	const FTransform SpawnTM = MerchantSpawnPoint->GetComponentTransform();

	SpawnedMerchant = GetWorld()->SpawnActor<AClcMerchant>(
		AClcMerchant::StaticClass(),
		SpawnTM.GetLocation(),
		SpawnTM.GetRotation().Rotator(),
		Params);

	if (SpawnedMerchant)
	{
		SpawnedMerchant->Initialize(this);
	}
}

void AClcStoneStall::NotifyStoneRemoved(AClcStone* Stone)
{
	if (!IsValid(Stone)) return;

	// 算购买结果：这块的价值 vs 比较基线。
	// 修复：原来均值把被买走的这块也算进去，摊位只剩这一块时均值=自身 → 恒判 TookGood（哪怕废石）。
	const float StoneValue = Stone->GetStoneData().Internal.TheoreticalValue;

	// 基线优先取「其余仍在摊位的石头」均值（玩家挑走的是否比剩下的好）。
	float OthersTotal = 0.0f;
	int32 OthersCount = 0;
	for (AClcStone* S : SpawnedStones)
	{
		if (S != Stone && IsValid(S))
		{
			OthersTotal += S->GetStoneData().Internal.TheoreticalValue;
			++OthersCount;
		}
	}

	float Baseline;
	if (OthersCount > 0)
	{
		Baseline = OthersTotal / static_cast<float>(OthersCount);
	}
	else if (BoughtStoneCount > 0)
	{
		// 摊位只剩这最后一块：没有剩下的可比，改用「此前已售出石头」的均值作基线。
		Baseline = SumBoughtStoneValues / static_cast<float>(BoughtStoneCount);
	}
	else
	{
		// 单石摊位、首次购买、无任何历史：无基线可比，维持原行为（不冤枉好货）。
		Baseline = StoneValue;
	}

	const EClcPurchaseOutcome Outcome = (StoneValue >= Baseline)
		? EClcPurchaseOutcome::TookGood
		: EClcPurchaseOutcome::TookBad;

	// 记入历史，供后续「最后一块」比较使用。
	SumBoughtStoneValues += StoneValue;
	++BoughtStoneCount;

	// 从数组移除——修复原来 RemoveFromStall 直接 Destroy 不通知的悬挂指针问题
	SpawnedStones.Remove(Stone);

	// 广播给商人（及其他监听者）
	OnStoneRemoved.Broadcast(Outcome);
}

void AClcStoneStall::CollectSlots(TArray<FClcSlotSaveState>& OutSlots) const
{
	for (int32 i = 0; i < SpawnedStones.Num(); ++i)
	{
		AClcStone* Stone = SpawnedStones[i];
		if (!IsValid(Stone)) continue;
		const FClcStoneRuntimeData& RT = Stone->GetStoneData();
		FClcSlotSaveState& Slot = OutSlots.AddDefaulted_GetRef();
		Slot.SlotIndex = i;
		Slot.bSold = false;
		Slot.InternalData = RT.Internal;
		Slot.EffectiveScale = Stone->GetActorScale3D().GetMax();
		Slot.EffectivePurchasePrice = RT.Internal.PurchasePrice;
		Slot.DisplayName = RT.DisplayName;
	}
}

void AClcStoneStall::RestoreFromSlots(const TArray<FClcSlotSaveState>& Slots)
{
	// 先清空 BeginPlay 生成的随机批次石头，再用存档数据还原
	for (AClcStone* Stone : SpawnedStones)
	{
		if (Stone) Stone->Destroy();
	}
	SpawnedStones.Empty();
	SumBoughtStoneValues = 0.0f;
	BoughtStoneCount = 0;

	int32 Cols, Rows;
	if (!CalcGridLayout(Slots.Num(), Cols, Rows)) return;

	UClcStallConfig* StallCfg = MarketSubsystem ? MarketSubsystem->GetStallConfig() : nullptr;
	UClcStoneMeshConfig* MeshCfg = MarketSubsystem ? MarketSubsystem->GetMeshConfig() : nullptr;
	if (!StallCfg || !MeshCfg) return;

	for (const FClcSlotSaveState& Slot : Slots)
	{
		const int32 i = Slot.SlotIndex;
		const int32 Col = i % Cols;
		const int32 Row = i / Cols;

		const float CellSize = StallCfg->UnitCellSize;
		const float OffsetX = (Col - (Cols - 1) * 0.5f) * CellSize;
		const float OffsetY = (Row - (Rows - 1) * 0.5f) * CellSize;

		const FVector Center = StoneSpawnCenter->GetComponentLocation();
		const FQuat SpawnQuat = StoneSpawnCenter->GetComponentQuat();

		bool bSuccess = false;
		FClcStoneInternalData InternalData = Slot.InternalData;
		InternalData.PurchasePrice = Slot.EffectivePurchasePrice;
		InternalData.MeshScale = Slot.EffectiveScale;
		InternalData.DistributionMap.Data.Empty(); // 运行时按需重建

		// 优先用存档里的原始 Mesh（StoneMesh 生成时确定），加载失败才随机兜底
		UStaticMesh* Mesh = Slot.InternalData.StoneMesh.LoadSynchronous();
		if (!Mesh && MeshCfg)
		{
			Mesh = MeshCfg->GetRandomMesh();
		}
		const float MeshRadius = Mesh ? Mesh->GetBounds().SphereRadius : 50.0f;
		const float Scale = FMath::Min(Slot.EffectiveScale, CellSize / (MeshRadius * 2.0f));

		if (Mesh)
		{
			const FBoxSphereBounds Bounds = Mesh->GetBounds();
			const float HalfHeight = Bounds.Origin.Z + Bounds.BoxExtent.Z;
			const FVector SpawnLoc = Center + SpawnQuat.RotateVector(FVector(OffsetX, OffsetY, HalfHeight * Scale));
			const float Yaw = FMath::FRandRange(0.0f, 360.0f);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AClcStone* Stone = GetWorld()->SpawnActor<AClcStone>(AClcStone::StaticClass(), SpawnLoc, FRotator(0, Yaw, 0), Params);
			if (Stone)
			{
				InternalData.MeshScale = Scale;
				Stone->Initialize(InternalData, Mesh, Scale, Slot.DisplayName);
				Stone->SetOwningStall(this);
				Stone->ApplyInteractionConfig(StallCfg->StoneInteractionRadius, StallCfg->StoneAimSweepRadius);
				SpawnedStones.Add(Stone);
			}
		}
	}
	if (SpawnedMerchant) SpawnedMerchant->RecomputeTier();
}

// ---- 价格封顶 & 换批 ----

void AClcStoneStall::RefreshAllStalls()
{
	// 重新生成石头（SpawnStones 内部先销毁旧石头 → 生成 → ClampAllStones）
	SpawnStones();

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcStoneStall] RefreshAllStalls done —— %s, target=%d, max=%d"),
		*GetName(), TargetPurchasePrice, GetMaxAllowedPrice());
}

int32 AClcStoneStall::GetMaxAllowedPrice() const
{
	return FMath::RoundToInt(static_cast<float>(TargetPurchasePrice) * PriceOverflowFactor);
}

void AClcStoneStall::ClampStonePriceIfNeeded(AClcStone* Stone)
{
	const int32 MaxPrice = GetMaxAllowedPrice();
	const int32 CurrentPrice = Stone->GetStoneData().Internal.PurchasePrice;

	if (CurrentPrice <= MaxPrice) return;

	// 等比缩小：PurchasePrice ∝ SurfaceArea ∝ Scale²
	// → ScaleFactor = sqrt(MaxPrice / CurrentPrice)，不低于 MinStoneScaleRatio
	const float ScaleFactor = FMath::Sqrt(
		static_cast<float>(MaxPrice) / static_cast<float>(CurrentPrice));
	const float ClampedFactor = FMath::Max(ScaleFactor, MinStoneScaleRatio);

	const float CurScale = Stone->GetActorScale3D().GetMax();
	const float NewScale = CurScale * ClampedFactor;
	Stone->SetActorScale3D(FVector(NewScale));
	Stone->GetStoneData().Internal.MeshScale = NewScale;

	// 缩放变动后重新贴地——底部对齐桌面顶面
	const float TableTopZ = StoneSpawnCenter->GetComponentLocation().Z;
	Stone->SnapToSurface(TableTopZ);

	Stone->RecalculateSurfaceArea();
	Stone->RecalculatePrices();

#if !UE_BUILD_SHIPPING
	UE_LOG(LogClaudeCore, Verbose,
		TEXT("[ClcStoneStall] Clamped stone %s: %d → %d (scale %.2f → %.2f, factor=%.3f)"),
		*Stone->GetName(), CurrentPrice, Stone->GetStoneData().Internal.PurchasePrice,
		CurScale, NewScale, ClampedFactor);
#endif
}

void AClcStoneStall::ClampAllStones()
{
	for (AClcStone* Stone : SpawnedStones)
	{
		if (IsValid(Stone))
		{
			ClampStonePriceIfNeeded(Stone);
		}
	}

	UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcStoneStall] Clamp done —— %s, %d stones, target=%d, max=%d"),
		*GetName(), SpawnedStones.Num(), TargetPurchasePrice, GetMaxAllowedPrice());
}
