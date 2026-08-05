// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ClcMerchantConfig.generated.h"

class UClcMerchantAnimConfig;
class UClcMerchantBubbleConfig;
class UClcMerchantBubbleWidget;
class UClcMerchantEagleEyeWidget;
class UClcMerchantTalkConfig;
class UClcMerchantPersonality;
class USkeletalMesh;
class UAnimInstance;
class UAnimMontage;

/** 商人屏幕空间 UI 的模拟透视参数——按相机距离将 UI 缩放为近大远小。 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcMerchantUISimulatedPerspectiveSettings
{
	GENERATED_BODY()

	/** 是否按相机距离模拟近大远小；关闭时保持原始尺寸。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective")
	bool bEnabled = true;

	/** 到此距离及以内使用 NearScale（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective", meta = (ClampMin = "0.0"))
	float NearDistance = 300.f;

	/** 到此距离及以外使用 FarScale（cm），必须大于 NearDistance。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective", meta = (ClampMin = "1.0"))
	float FarDistance = 2500.f;

	/** 靠近商人时的 UI 缩放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective", meta = (ClampMin = "0.1"))
	float NearScale = 1.10f;

	/** 远离商人时的 UI 缩放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective", meta = (ClampMin = "0.1"))
	float FarScale = 0.70f;
};

/**
 * 商人主配置——摆位参数、视觉资源、气泡参数、时序、档位阈值、子配置引用。
 * 全部 EditAnywhere，编辑器内直接调。
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcMerchantConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// ---- UI ----

	/** 口头气泡锚点相对商人骨骼网格的局部偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Talk")
	FVector TalkBubbleAnchorOffset = FVector(0.f, 0.f, 180.f);

	/** 鹰眼洞察锚点相对商人骨骼网格的局部偏移 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|EagleEye")
	FVector EagleEyeAnchorOffset = FVector(0.f, 0.f, 210.f);

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

	// ---- UI ----

	/** 口头气泡 Widget 类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Talk")
	TSubclassOf<UClcMerchantBubbleWidget> TalkBubbleWidgetClass;

	/** 鹰眼洞察 Widget 类 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|EagleEye")
	TSubclassOf<UClcMerchantEagleEyeWidget> EagleEyeWidgetClass;

	/** 口头气泡与鹰眼洞察共用的 2D 模拟透视参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|Perspective")
	FClcMerchantUISimulatedPerspectiveSettings UISimulatedPerspective;

	// ---- 屏幕外指示器（Off-Screen Indicator） ----

	/** 气泡离屏时是否钳到屏幕边缘并显示指向箭头（关闭=旧行为，离屏直接隐藏）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|OffScreen")
	bool bEnableOffScreenIndicator = true;

	/** 气泡外缘到屏幕边缘的像素留白——气泡完整保持在屏内，指向箭头落在此间隙的正中。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|OffScreen", meta = (ClampMin = "0.0"))
	float OffScreenEdgeMargin = 60.f;

	/** 离屏钳制时气泡的固定缩放（HUD 指示器需稳定可读，不随距离缩）；1.0=原始尺寸。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI|OffScreen", meta = (ClampMin = "0.1"))
	float OffScreenScale = 1.0f;

	// ---- 时序 ----

	/** 动画切换 blend 时长（秒，0=瞬切） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float AnimBlendTime = 0.3f;

	/** 微反应持续时间（秒，后回到 mood） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	float MicroReactionDuration = 2.5f;

	/** 购买反馈气泡的最低显示时间（秒），期间瞄准变化不会覆盖文本 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.1"))
	float PurchaseFeedbackDuration = 1.8f;

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
