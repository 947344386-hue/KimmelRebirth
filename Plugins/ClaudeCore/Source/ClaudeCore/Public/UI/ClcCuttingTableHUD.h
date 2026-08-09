// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcCuttingTableHUD.generated.h"

class UTextBlock;
class UProgressBar;

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
	float RemovedVolume = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Cut")
	float RemainingVolume = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "CuttingHUD|Gold")
	int32 SettledGold = 0;

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
	TObjectPtr<UTextBlock> RemovedVolumeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> RemainingVolumeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> PositionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> BladeText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UProgressBar> BladeProgressBar;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> CutStateText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "CuttingHUD")
	TObjectPtr<UTextBlock> HintsText;

private:
	void BuildDefaultLayout();
};
