// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ClcDeveloperSettings.generated.h"

/**
 * ClaudeCore 插件全局配置——Project Settings → Plugins → ClaudeCore。
 * 集中管理所有 DataAsset 资产路径，挪资产/换项目只改这里，不动 C++ 代码。
 *
 * 默认值是约定路径（/Game/JadeBetting/Data/），开箱即用。
 * 设计师在 Project Settings 里改路径即可适配不同项目结构。
 */
UCLASS(Config=Game, defaultconfig, meta=(DisplayName="ClaudeCore"))
class CLAUDECORE_API UClcDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// ---- DataAsset 路径 ----

	/** 石头主配置（种水概率/定价系数/初始金币等） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets", meta=(ToolTip="石头主配置 DataAsset 路径"))
	FString StoneConfigPath = TEXT("/Game/JadeBetting/Data/DA_StoneConfig");

	/** 石头 Mesh 配置（可选 Mesh 池） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets")
	FString StoneMeshConfigPath = TEXT("/Game/JadeBetting/Data/DA_StoneMeshConfig");

	/** 摊位配置（每摊石头数/格子大小/缩放范围等） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets")
	FString StallConfigPath = TEXT("/Game/JadeBetting/Data/DA_StallConfig");

	/** 皮壳纹理配置（皮壳名/贴图/粗糙度等） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets")
	FString ShellTextureConfigPath = TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig");

	/** 玉石纹理配置（开窗后内部材质贴图） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets")
	FString JadeTextureConfigPath = TEXT("/Game/JadeBetting/Data/DA_JadeTextureConfig");

	/** 鹰眼技能配置（持续时间/冷却/扫描间隔等） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets")
	FString EagleEyeConfigPath = TEXT("/Game/JadeBetting/Data/DA_EagleEyeConfig");

	// ---- 商人系统 ----

	/** 商人主配置（摆位/视觉/时序/档位阈值） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets|Merchant", meta=(ToolTip="商人主配置 DataAsset 路径"))
	FString MerchantConfigPath = TEXT("/Game/JadeBetting/Data/DA_MerchantConfig");

	/** 商人动画池配置（动画引用按状态分池） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets|Merchant")
	FString MerchantAnimConfigPath = TEXT("/Game/JadeBetting/Data/DA_MerchantAnimConfig");

	/** 商人气泡文字池配置（9 状态文字池） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets|Merchant")
	FString MerchantBubbleConfigPath = TEXT("/Game/JadeBetting/Data/DA_MerchantBubbleConfig");

	// ---- 讨价还价 QTE ----

	/** 讨价还价 QTE 配置（上浮档位/每键窗口/文案模板） */
	UPROPERTY(Config, EditAnywhere, Category="DataAssets|Haggle", meta=(ToolTip="讨价还价 QTE 配置 DataAsset 路径"))
	FString HaggleConfigPath = TEXT("/Game/JadeBetting/Data/DA_HaggleConfig");
};
