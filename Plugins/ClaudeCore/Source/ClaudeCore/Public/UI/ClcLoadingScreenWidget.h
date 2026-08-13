// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SImage;
class STextBlock;
struct FSlateDynamicImageBrush;
struct FSoftObjectPath;

/**
 * 引擎级加载画面 Slate widget —— 轮播背景图 + 加载文案 + 随机提示。
 *
 * 注入 MoviePlayer 的 FLoadingScreenAttributes.WidgetLoadingScreen。
 * MoviePlayer 只在打包游戏生效（!GIsEditor），PIE 里不显示，不影响自动保存。
 * Tick 由 MoviePlayer::WaitForMovieToFinish 循环里的 SlateApp.Tick() 驱动——
 * 这正是选 MoviePlayer 而非普通 UMG widget 的原因：UMG 在 OpenLevel 阻塞期间不 Tick。
 *
 * 配置（UClcDeveloperSettings，Project Settings → Plugins → ClaudeCore → Loading）：
 *   - LoadingBackgrounds：背景图库（FSoftObjectPath 数组，软引用，轮播到时才同步加载）
 *   - LoadingBackgroundSwitchInterval：轮播间隔秒（默认 3.0）
 *   - LoadingTips：提示文案；为空则用 C++ 内置默认池
 * 空图库时显示纯暗底 + 文案 + 提示，不崩。
 */
class CLAUDECORE_API SClcLoadingScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClcLoadingScreenWidget) {}
	SLATE_END_ARGS()

	SClcLoadingScreenWidget();
	~SClcLoadingScreenWidget();

	void Construct(const FArguments& InArgs);

	// SCompoundWidget override —— MoviePlayer tick pump 驱动轮播
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime) override;

private:
	/** 随机选起始背景图索引并立即加载显示 */
	void InitFirstBackground();

	/** 加载指定索引的背景图并设到 BackgroundImage；加载失败则保持当前图 */
	void LoadBackgroundAt(int32 Index);

	/** 切到下一张背景图并加载其 brush */
	void AdvanceBackground();

	/** 从配置或内置默认池随机选一条提示 */
	FString PickRandomTip() const;

	// 背景图库（从 DeveloperSettings 拷贝一份）
	TArray<FSoftObjectPath> Backgrounds;
	int32 CurrentBgIndex = INDEX_NONE;
	float SwitchInterval = 3.0f;
	float TimeSinceSwitch = 0.0f;

	/** 当前背景 brush —— 持有以保证 SetImage 传入的指针长期有效 */
	TSharedPtr<FSlateDynamicImageBrush> CurrentBrush;

	TSharedPtr<SImage> BackgroundImage;
	TSharedPtr<STextBlock> LoadingText;
	TSharedPtr<STextBlock> TipText;
};
