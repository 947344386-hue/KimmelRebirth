// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Quest/ClcQuestTypes.h"
#include "ClcQuestConfig.generated.h"

/**
 * 任务配置 DataAsset —— 持有全部任务定义。
 *
 * 蓝图用法：Content 中创建继承本类的 DA_QuestConfig，在 Quests 数组里填任务，
 * 按 NextQuestID 串成主/支线链。新游戏开局 QuestSubsystem 按 ID 自动接取所有链首。
 *
 * 约定：链首任务 = QuestID 在整个 Quests 数组的 NextQuestID 字段中未被任何任务引用过的任务。
 * 换言之，"没人指向我"的任务就是链首，会被自动接取。
 */
UCLASS()
class CLAUDECORE_API UClcQuestConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 全部任务定义。配表时按 NextQuestID 串链。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    TArray<FClcQuestData> Quests;
};
