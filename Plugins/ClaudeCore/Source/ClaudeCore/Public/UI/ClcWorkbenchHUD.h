// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcWorkbenchHUD.generated.h"

/**
 * Workbench HUD 每帧数据包——C++ 组装后 Push 给 BP 渲染。
 */
USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcWorkbenchHUDData
{
	GENERATED_BODY()

	// ── 石头信息 ──

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	FString Origin;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	FString ShellName;

	/** 是否已暴露出玉石（首次开到绿时变 true，在此之前种水隐藏） */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	bool bGradeRevealed = false;

	/** EClcJadeGrade cast 过来的索引，BP 侧自行查表映射文字和颜色 */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	uint8 GradeValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	int32 PurchasePrice = 0;

	/** 卖价趋势：1=赚(↑)  0=持平  -1=亏(↓) */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Stone")
	int32 ValuationTrend = 0;

	// ── 开窗 ──

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Opening")
	float OpenedRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Opening")
	float SurfaceArea = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Opening")
	float GreenArea = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Opening")
	float BlackArea = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Opening")
	int32 CurrentValuation = 0;

	// ── 工具 ──

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Tool")
	FString ToolName;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Tool")
	float ToolDurability = 1.0f;

	/** 手电筒模式下灯是否开启 */
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Tool")
	bool bToolActive = false;

	// ── 提示 ──

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Hints")
	FString OperationHints = TEXT("WASD 旋转 | T 切工具 | 左键 使用\nB 背包 | Esc 退出");
};

/**
 * Workbench HUD Widget——C++ 负责组装数据包，BP 负责排版渲染。
 */
UCLASS()
class CLAUDECORE_API UClcWorkbenchHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/** C++ 每帧推数据，BP 覆写此事件刷新 UI */
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "WorkbenchHUD")
	void RefreshData(const FClcWorkbenchHUDData& Data);
};
