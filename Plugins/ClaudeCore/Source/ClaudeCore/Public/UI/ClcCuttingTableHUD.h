// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcCuttingTableHUD.generated.h"

class UTextBlock;
class UProgressBar;

/** 切块尺寸预判四态——HUD 右上角实时显示，帮助玩家判断下刀是否合理 */
UENUM(BlueprintType)
enum class EClcCutSizeState : uint8
{
	CannotCut   UMETA(DisplayName = "无法下刀"),   // 刀口未穿过两侧 / 耐久不足 / 无石
	Undersized  UMETA(DisplayName = "切块过小"),   // 切下比例 < IdealCutRatioMin
	Standard    UMETA(DisplayName = "切块尺寸标准"), // 比例在 [Min,Max] 区间
	Oversized   UMETA(DisplayName = "切块过大"),   // 比例 > IdealCutRatioMax
};

USTRUCT(BlueprintType)
struct CLAUDECORE_API FClcCuttingTableHUDData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Stone")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Stone")
	FString Origin;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Stone")
	bool bGradeRevealed = false;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Stone")
	uint8 GradeValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Cut")
	int32 CutCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Cut")
	float CutProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Gold")
	int32 SettledGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Gold")
	int32 PurchasePrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Gold")
	int32 CurrentValuation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Position")
	float StoneOffset = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Position")
	float MovementRange = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Tool")
	float BladeDurability = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Tool")
	float BladeCurrent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Tool")
	float BladeMax = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Tool")
	bool bCanCut = false;

	/** 切块尺寸预判四态（供 CutStateText 文案+配色；CannotCut 时无比例） */
	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|CutSize")
	EClcCutSizeState CutSizeState = EClcCutSizeState::CannotCut;

	/** 当前刀口预判切下比例 [0,1]（供文案显示百分比；CannotCut 时无意义） */
	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|CutSize")
	float CutSizeRatio = 0.0f;

	/** 剩余主体是否可一键出售（体积已进入标准切割阈值区间） */
	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|CutSize")
	bool bCanSellRemaining = false;

	/** 剩余主体预估售价（供 HUD 提示显示） */
	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|CutSize")
	int32 RemainingSellPrice = 0;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Hints")
	FString OperationHints = TEXT("A / D 移动原石 | 空格 下刀\nB 更换原石 | Esc 退出");
};

UCLASS()
class CLAUDECORE_API UClcCuttingTableHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcCuttingTableHUD(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "CuttingHUD")
	void RefreshData(const FClcCuttingTableHUDData& Data);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> DisplayNameText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> OriginText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> GradeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> CutCountText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UProgressBar> CutProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> PositionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UProgressBar> BladeProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> CutStateText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> ValuationText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> HintsText;

private:
	void BuildDefaultLayout();
};
