// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class STextBlock;
class FDeferredCleanupSlateBrush;
struct FSoftObjectPath;

/** 引擎级加载画面 Slate widget —— 单张随机背景图 + 加载文案 + 随机提示。 */
class CLAUDECORE_API SClcLoadingScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClcLoadingScreenWidget) {}
	SLATE_END_ARGS()

	SClcLoadingScreenWidget();
	~SClcLoadingScreenWidget();

	void Construct(const FArguments& InArgs);

private:
	/** 随机选择并加载一张背景图 */
	void InitFirstBackground();

	/** 加载指定索引的背景图并设到 BackgroundImage；加载失败则保持当前图 */
	void LoadBackgroundAt(int32 Index);

	/** 从配置或内置默认池随机选一条提示 */
	FString PickRandomTip() const;

	TArray<FSoftObjectPath> Backgrounds;

	/**
	 * 当前背景 brush —— 用 FDeferredCleanupSlateBrush 而非 FSlateDynamicImageBrush。
	 * 引擎在 SlateDynamicImageBrush.h 明确禁止加载屏使用后者（独立 exe 启动即 ensure 失败），
	 * FDeferredCleanupSlateBrush 正确处理 GC 生命周期与 Slate 渲染管线的多帧延迟释放。
	 */
	TSharedPtr<FDeferredCleanupSlateBrush> CurrentBrush;

	TSharedPtr<SImage> BackgroundImage;
	TSharedPtr<STextBlock> LoadingText;
	TSharedPtr<STextBlock> TipText;
};
