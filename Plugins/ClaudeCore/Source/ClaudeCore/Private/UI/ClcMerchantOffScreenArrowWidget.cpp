// Copyright ClaudeCore. All Rights Reserved.

#include "UI/ClcMerchantOffScreenArrowWidget.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"

void UClcMerchantOffScreenArrowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildDefaultLayout();

	// 中心对齐 + 中心枢轴：SetPositionInViewport 传入即为中心坐标，旋转绕中心
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	if (ArrowImage)
	{
		if (UTexture2D* Tex = EnsureDefaultArrowTexture())
		{
			ArrowImage->SetBrushFromTexture(Tex);
		}
		ArrowImage->SetColorAndOpacity(FLinearColor::White);
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void UClcMerchantOffScreenArrowWidget::BuildDefaultLayout()
{
	// 无蓝图提供 RootWidget 时，C++ 构造固定尺寸 SizeBox + 三角 Image
	if (!WidgetTree || WidgetTree->RootWidget) return;

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ArrowRoot"));
	Root->SetWidthOverride(32.f);
	Root->SetHeightOverride(32.f);
	WidgetTree->RootWidget = Root;

	ArrowImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ArrowImage"));
	Root->AddChild(ArrowImage);
}

void UClcMerchantOffScreenArrowWidget::ShowAt(const FVector2D& ScreenPos, float AngleDeg)
{
	if (GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	SetPositionInViewport(ScreenPos);
	SetRenderTransformAngle(AngleDeg);
}

void UClcMerchantOffScreenArrowWidget::Hide()
{
	if (GetVisibility() != ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

UTexture2D* UClcMerchantOffScreenArrowWidget::EnsureDefaultArrowTexture()
{
	if (CachedArrowTexture) return CachedArrowTexture;

	constexpr int32 Size = 32;
	// PF_B8G8R8A8：内存字节序 B,G,R,A；白色不透明 = (255,255,255,255)
	CachedArrowTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!CachedArrowTexture) return nullptr;

	CachedArrowTexture->MipGenSettings = TMGS_NoMipmaps;
	CachedArrowTexture->SRGB = true;

	FTexture2DMipMap& Mip = CachedArrowTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (Data)
	{
		uint8* Pixels = static_cast<uint8*>(Data);
		// 朝右(+X)三角形顶点（纹理空间 x 右、y 下）
		const float Ax = 4.f,  Ay = 2.f;
		const float Bx = 31.f, By = 16.f;
		const float Cx = 4.f,  Cy = 30.f;
		for (int32 y = 0; y < Size; ++y)
		{
			for (int32 x = 0; x < Size; ++x)
			{
				const float Px = static_cast<float>(x) + 0.5f;
				const float Py = static_cast<float>(y) + 0.5f;
				const float e1 = (Bx - Ax) * (Py - Ay) - (By - Ay) * (Px - Ax);
				const float e2 = (Cx - Bx) * (Py - By) - (Cy - By) * (Px - Bx);
				const float e3 = (Ax - Cx) * (Py - Cy) - (Ay - Cy) * (Px - Cx);
				const bool bInside = (e1 >= 0.f && e2 >= 0.f && e3 >= 0.f)
					|| (e1 <= 0.f && e2 <= 0.f && e3 <= 0.f);
				const int32 Idx = (y * Size + x) * 4;
				if (bInside)
				{
					Pixels[Idx + 0] = 255; // B
					Pixels[Idx + 1] = 255; // G
					Pixels[Idx + 2] = 255; // R
					Pixels[Idx + 3] = 255; // A
				}
				else
				{
					Pixels[Idx + 0] = 0;
					Pixels[Idx + 1] = 0;
					Pixels[Idx + 2] = 0;
					Pixels[Idx + 3] = 0;
				}
			}
		}
	}
	Mip.BulkData.Unlock();
	CachedArrowTexture->UpdateResource();
	return CachedArrowTexture;
}
