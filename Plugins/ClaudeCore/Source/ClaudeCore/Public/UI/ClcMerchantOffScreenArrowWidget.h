// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ClcMerchantOffScreenArrowWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * 离屏指向箭头——独立屏幕空间 Widget，由口头气泡拥有。
 * 定位在「气泡外缘 ↔ 屏幕边缘」间隙的正中，按商人屏外方位旋转。
 * 与气泡分离（不做子控件），避免子控件裁剪 / 父级缩放复合 / 槽位锚点不确定性。
 */
UCLASS()
class CLAUDECORE_API UClcMerchantOffScreenArrowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 显示并定位到指定屏幕坐标（中心对齐），按商人屏外方位旋转（度，0=朝右）。 */
	void ShowAt(const FVector2D& ScreenPos, float AngleDeg);

	/** 隐藏。 */
	void Hide();

protected:
	virtual void NativeOnInitialized() override;

private:
	/** 构造默认布局：固定尺寸 SizeBox + 一张三角箭头 Image（无蓝图时由 C++ 生成）。 */
	void BuildDefaultLayout();

	/** 懒生成 32×32 朝右白色三角贴图，赋给箭头 Image。 */
	UTexture2D* EnsureDefaultArrowTexture();

	/** 箭头图片（C++ 默认布局生成）。 */
	UPROPERTY(Transient)
	TObjectPtr<UImage> ArrowImage = nullptr;

	/** 程序化三角贴图缓存（只生成一次）。 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedArrowTexture = nullptr;
};
