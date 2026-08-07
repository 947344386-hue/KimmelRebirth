// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcGoldFlyWidget.generated.h"

class UTextBlock;

UCLASS()
class CLAUDECORE_API UClcGoldFlyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UClcGoldFlyWidget(const FObjectInitializer& ObjectInitializer);

	void StartFlight(const FVector2D& ScreenFrom, int32 GoldAmount);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GoldLabel;

	FVector2D From;
	FVector2D To;
	float Elapsed = 0.0f;
	static constexpr float Duration = 0.55f;
	bool bFlying = false;
	int32 Amount = 0;

	void BuildDefaultLayout();
};