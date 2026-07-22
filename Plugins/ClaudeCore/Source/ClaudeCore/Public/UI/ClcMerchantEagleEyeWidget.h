// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/ClcMerchantBubbleWidget.h"
#include "ClcMerchantEagleEyeWidget.generated.h"

/** 鹰眼洞察 UI——独立显示商人的心理话和性格，不占用口头气泡实例。 */
UCLASS()
class CLAUDECORE_API UClcMerchantEagleEyeWidget : public UClcMerchantBubbleWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantEagleEye")
	void SetInsightText(const FText& Text) { SetBubbleText(Text); }

	UFUNCTION(BlueprintCallable, Category = "ClcMerchantEagleEye")
	void SetPersonalityText(const FText& Text) { SetSecondaryText(Text); }

	/** 设性格行颜色——主行 BubbleTextBlock 着色（越邪恶越紫/越善良越青） */
	UFUNCTION(BlueprintCallable, Category = "ClcMerchantEagleEye")
	void SetPersonalityColor(const FLinearColor& Color) { if (BubbleTextBlock) BubbleTextBlock->SetColorAndOpacity(Color); }
};
