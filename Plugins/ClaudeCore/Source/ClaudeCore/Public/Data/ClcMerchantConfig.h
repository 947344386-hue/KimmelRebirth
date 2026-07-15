// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcMerchantConfig.generated.h"

class UClcMerchantAnimConfig;
class UClcMerchantBubbleConfig;
class UClcMerchantBubbleWidget;
class UClcMerchantTalkConfig;
class UClcMerchantPersonality;
class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;

/**
 * 商人主配置——摆位参数、视觉资源、气泡参数、时序、档位阈值、子配置引用。
 * 全部 EditAnywhere，编辑器内直接调。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- 气泡 ----

	/** 气泡锚点相对商人的世界偏移（头顶） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bubble")
	FVector BubbleAnchorOffset = FVector(0.f, 0.f, 180.f);

	// ---- 视觉 ----

	/** 商人骨骼网格体池——Initialize 时随机抽一条；留空则需在 BP 子类里指定 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TArray<USkeletalMesh*> MerchantMeshPool;

	/** 动画实例类（带 DefaultSlot 才能播 dynamic montage 融合；空则走 PlayAnimation 兜底无融合） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<UAnimInstance> AnimInstanceClass;

	// ---- 摆位 ----

	/** 贴地 trace 距离——spawn 后从当前位置向下 trace 此距离找地面落 Z */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float GroundSnapTraceDistance = 300.f;

	/** 骨骼面朝偏移（度）——箭头朝向代表商人面朝方向，骨骼自身面朝可能差 yaw，在此补正。
	 *  各套骨骼面朝不一致时需升级为 per-mesh offset（当前全局） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	float MeshFacingYawOffset = 0.f;

	// ---- 气泡 ----

	/** 气泡 Widget 类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bubble")
	TSubclassOf<UClcMerchantBubbleWidget> BubbleWidgetClass;

	/** 气泡相对商人头顶投影的屏幕像素偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bubble")
	FVector2D BubbleScreenOffset = FVector2D(0.f, -60.f);

	// ---- 时序 ----

	/** 动画切换 blend 时长（秒，0=瞬切） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float AnimBlendTime = 0.3f;

	/** 微反应持续时间（秒，后回到 mood） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float MicroReactionDuration = 2.5f;

	/** mood 动画重抽间隔（秒，0=每次播完才重抽） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float MoodReshuffleInterval = 0.f;

	// ---- 档位判定 ----

	/** 摊位剩余石头综合价值 >= 此值 = good */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
	float GoodTierThreshold = 5000.f;

	/** <= 此值 = bad */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tier", meta = (ClampMin = "0.0"))
	float BadTierThreshold = 1500.f;

	// ---- 子配置引用 ----

	/** 动画池配置 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configs")
	UClcMerchantAnimConfig* AnimConfig = nullptr;

	/** 气泡文字池配置（心理话，鹰眼限时，诚实） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configs")
	UClcMerchantBubbleConfig* BubbleConfig = nullptr;

	/** 嘴上话术池配置（对外推销，可骗，走近显示） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Configs")
	UClcMerchantTalkConfig* TalkConfig = nullptr;

	// ---- 性格 ----

	/** 性格池——Initialize 时随机 roll 一个；空池则无性格（不撒谎/不藏动作） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality")
	TArray<UClcMerchantPersonality*> PersonalityPool;

	/** 嘴上话术 TriggerSphere 半径——玩家进入此范围显示嘴上气泡 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Personality", meta = (ClampMin = "100.0"))
	float TalkTriggerRadius = 400.0f;
};
