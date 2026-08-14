// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/ClcJadeTypes.h"
#include "Quest/ClcQuestTypes.h"
#include "ClcDeveloperSettings.h"
#include "ClcSessionTypes.generated.h"

/**
 * 摊位存档槽位——每块石头的完整数据。
 * 不存世界坐标——生成时按摊位局部网格公式重建位置。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcSlotSaveState
{
	GENERATED_BODY()

	/** 槽位索引（0~StonesPerStall-1） */
	UPROPERTY(SaveGame)
	int32 SlotIndex = 0;

	/** 是否已售出 */
	UPROPERTY(SaveGame)
	bool bSold = false;

	/** 完整内部数据（Seed/种水/产地/皮壳等，含 DistributionMap 重建所需参数） */
	UPROPERTY(SaveGame)
	FClcStoneInternalData InternalData;

	/** 封顶后有效缩放（唯一真源：同时 = ActorScale 和 Internal.MeshScale） */
	UPROPERTY(SaveGame)
	float EffectiveScale = 1.0f;

	/** 封顶后有效购买价（保存前已由 Stall 价格封顶缩价后的终值） */
	UPROPERTY(SaveGame)
	int32 EffectivePurchasePrice = 0;

	/** 展示名 */
	UPROPERTY(SaveGame)
	FString DisplayName;
};

/**
 * 摊位存档状态——按 StallId 分组存储，读档时每个摊位只恢复自己的 Slots。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcStallSaveState
{
	GENERATED_BODY()

	/** 关卡中唯一标识（对应 GetPathName，与 AClcStoneStall::GetStallId() 一致） */
	UPROPERTY(SaveGame)
	FName StallId;

	/** 各槽位状态 */
	UPROPERTY(SaveGame)
	TArray<FClcSlotSaveState> Slots;
};

/**
 * 难度预设——影响起始金币、购买溢价系数、衰减权重等
 */
UENUM(BlueprintType)
enum class EClcDifficultyPreset : uint8
{
	Easy    UMETA(DisplayName = "简单模式"),
	Normal  UMETA(DisplayName = "标准模式"),
	Hard    UMETA(DisplayName = "困难模式"),
	Custom  UMETA(DisplayName = "自定义"),
};

/** 主菜单起始金币滑条的有效规则（由 DeveloperSettings 归一化）。 */
struct FClcStartingGoldRules
{
	int32 Min = 20000;
	int32 Max = 100000;
	int32 Step = 5000;
	int32 Default = 50000;

	int32 Snap(int32 Value) const
	{
		const int32 ClampedValue = FMath::Clamp(Value, Min, Max);
		const double StepOffset =
			(static_cast<double>(ClampedValue) - static_cast<double>(Min)) / static_cast<double>(Step);
		const int64 SnappedValue = static_cast<int64>(Min)
			+ FMath::RoundToInt64(StepOffset) * static_cast<int64>(Step);
		return static_cast<int32>(FMath::Clamp<int64>(SnappedValue, Min, Max));
	}
};

inline FClcStartingGoldRules ClcGetStartingGoldRules()
{
	FClcStartingGoldRules Rules;
	if (const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>())
	{
		Rules.Min = FMath::Max(0, DS->StartingGoldMin);
		Rules.Step = FMath::Max(1, DS->StartingGoldStep);

		const int64 RequestedMax = FMath::Max<int64>(Rules.Min, DS->StartingGoldMax);
		const int64 ReachableMax = static_cast<int64>(Rules.Min)
			+ ((RequestedMax - Rules.Min) / Rules.Step) * Rules.Step;
		Rules.Max = static_cast<int32>(FMath::Min<int64>(ReachableMax, MAX_int32));
		Rules.Default = Rules.Snap(DS->StartingGoldDefault);
	}
	return Rules;
}

/** 预设对应的黄金乘数（读 UClcDeveloperSettings） */
inline float ClcDifficultyGoldMultiplier(EClcDifficultyPreset Preset)
{
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (!DS) return 1.0f;
	switch (Preset)
	{
	case EClcDifficultyPreset::Easy:   return DS->DifficultyEasyGoldMultiplier;
	case EClcDifficultyPreset::Normal: return DS->DifficultyNormalGoldMultiplier;
	case EClcDifficultyPreset::Hard:   return DS->DifficultyHardGoldMultiplier;
	default: return 1.0f;
	}
}

/** 预设对应的溢价/衰减惩罚乘数（越高越难，读 UClcDeveloperSettings） */
inline float ClcDifficultyPenaltyMultiplier(EClcDifficultyPreset Preset)
{
	const UClcDeveloperSettings* DS = GetDefault<UClcDeveloperSettings>();
	if (!DS) return 1.0f;
	switch (Preset)
	{
	case EClcDifficultyPreset::Easy:   return DS->DifficultyEasyPenaltyMultiplier;
	case EClcDifficultyPreset::Normal: return DS->DifficultyNormalPenaltyMultiplier;
	case EClcDifficultyPreset::Hard:   return DS->DifficultyHardPenaltyMultiplier;
	default: return 1.0f;
	}
}

/**
 * 会话配置——玩家在开局界面预设的参数。
 * 打包启动后由主菜单 Widget 填写，新游戏时注入子系统。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcSessionConfig
{
	GENERATED_BODY()

	/** 起始金币（实际默认值与滑条范围由 DeveloperSettings 提供） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	int32 StartingGold = 50000;

	/** 难度预设 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	EClcDifficultyPreset Difficulty = EClcDifficultyPreset::Normal;

	/** 难度系数（Custom 模式手动设；其他模式由预设自动填充） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float DifficultyMultiplier = 1.0f;

	/** 存档槽位名（空=新游戏未保存，非空=继续游戏） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString SaveSlotName;

	/** 是否新游戏（false=来自存档恢复） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	bool bIsNewGame = true;

	/** 游戏关卡路径（默认 Map_JadePlayTest） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Session")
	FString GameLevelPath = TEXT("/Game/JadeBetting/Level/Map_JadePlayTest");
};


/**
 * 存档全量数据——序列化到 USaveGame。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcSaveData
{
	GENERATED_BODY()

	// 背包
	UPROPERTY(SaveGame)
	TArray<struct FClcStoneRuntimeData> SavedStones;
	UPROPERTY(SaveGame)
	int32 SavedGold = 0;
	UPROPERTY(SaveGame)
	int32 SavedTotalEarned = 0;

	// 摊位——按 StallId 分组存储各摊位槽位数据（每个摊位只恢复自己那份）
	UPROPERTY(SaveGame)
	TMap<FName, FClcStallSaveState> SavedStalls;

	// 工具——用 int32 存 key（EClcRepairableTool/EClcToolUpgrade 的值，用于 UPROPERTY 序列化兼容）
	UPROPERTY(SaveGame)
	TMap<int32, float> SavedDurability;
	UPROPERTY(SaveGame)
	TMap<int32, float> SavedMaxDurability;
	UPROPERTY(SaveGame)
	TSet<int32> SavedUpgrades;

	// 任务
	UPROPERTY(SaveGame)
	TMap<FName, FClcQuestRuntimeState> SavedQuestStates;

	// 会话
	UPROPERTY(SaveGame)
	FClcSessionConfig SessionConfig;

	// 玩家位置/朝向（读档时恢复，新游戏用 PlayerStart）
	UPROPERTY(SaveGame)
	FVector SavedPlayerLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame)
	FRotator SavedPlayerRotation = FRotator::ZeroRotator;
	/** 是否有有效的玩家坐标（区分"从未存过"和"坐标 0,0,0"） */
	UPROPERTY(SaveGame)
	bool bHasPlayerTransform = false;

	// 元数据
	UPROPERTY(SaveGame)
	FDateTime SaveTimestamp;
	UPROPERTY(SaveGame)
	int32 SaveVersion = 0;  // 0=未设；新存档写 CURRENT_SAVE_VERSION(=3)
	UPROPERTY(SaveGame)
	FString LevelName;
	UPROPERTY(SaveGame)
	float PlayTimeHours = 0.0f;
};

UCLASS()
class CLAUDECORE_API UClcPlayerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FClcSaveData SaveData;
};

/** 当前存档版本号（整数，便于迁移链判断；旧字符串档反序列化后此字段取默认值 0，视为版本 0 拒绝加载） */
inline constexpr int32 ClcCurrentSaveVersion() { return 3; }

/**
 * 存档元数据——菜单存档列表展示用，不含全量数据。
 * 由 SaveManager 在枚举存档时读取 USaveGame 的元数据字段填充。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcSaveMetaData
{
	GENERATED_BODY()

	/** 槽位名（即文件名，不含路径/扩展名） */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString SlotName;

	/** 存档时间戳 */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SaveTimestamp;

	/** 存档时金币 */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Gold = 0;

	/** 存档时背包石头数 */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 StoneCount = 0;

	/** 累计游戏时长（小时） */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	float PlayTimeHours = 0.0f;

	/** 存档时所在关卡名（路径最后一段） */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString LevelName;

	/** 存档版本号 */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = 0;
};
