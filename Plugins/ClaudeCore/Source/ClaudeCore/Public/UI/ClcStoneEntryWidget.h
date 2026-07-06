// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStoneEntryWidget.generated.h"

class UClcStoneTooltipWidget;
class UClcStoneMarketSubsystem;
class UTextBlock;

/**
 * 背包石头单元格基类——把"缓存数据 + hover 显示/隐藏 tooltip"全封装进 C++。
 *
 * BP 子类（WBP_StoneEntry）只需：
 *   1. 父类改成 ClcStoneEntryWidget
 *   2. Class Defaults 里设 TooltipClass = WBP_StoneTooltip
 *   3. 不用写任何 OnMouseEnter/OnMouseLeave 逻辑——C++ 自动处理
 *
 * 数据流：BackpackWidget.RefreshDisplay 循环创建 StoneEntry 时调 InitializeEntry(Index, Data)，
 * 之后 hover 自动从缓存的 StoneData 构建 tooltip。
 */
UCLASS(Abstract)
class CLAUDECORE_API UClcStoneEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 初始化单元格——BackpackWidget.RefreshDisplay 循环里每个 entry 调一次。
	 * @param Index  在背包数组中的索引
	 * @param Data   这块石头的运行时数据（按值拷贝，后续背包数据变化不会自动同步——刷新时重建 entry）
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcStoneEntry")
	void InitializeEntry(int32 Index, const FClcStoneRuntimeData& Data);

	/** 拿索引（卖石头、选中石头时用） */
	UFUNCTION(BlueprintPure, Category = "ClcStoneEntry")
	int32 GetStoneIndex() const { return StoneIndex; }

	/** 拿缓存的石头数据（已初始化后才有效） */
	UFUNCTION(BlueprintPure, Category = "ClcStoneEntry")
	const FClcStoneRuntimeData& GetStoneData() const { return StoneData; }

	/** 清理当前 tooltip（如果存在）——背包关闭时由 BackpackWidget 遍历调用，防止残留 */
	void ClearTooltip() { DestroyTooltip(); }

protected:
	// ---- UUserWidget overrides：hover / 移除时自动管 tooltip ----

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InPointerEvent) override;
	virtual void NativeDestruct() override;

	/** 从父级移除时清理 tooltip——背包刷新重排时 entry 会被 RemoveFromParent，
	 *  tooltip 是 AddToViewport 的不会跟着销毁，必须在这里清掉，否则残留在屏幕上 */
	virtual void RemoveFromParent() override;

	// ---- 数据缓存 ----

	/** 在背包中的索引 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStoneEntry")
	int32 StoneIndex = -1;

	/** 缓存的石头数据 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcStoneEntry")
	FClcStoneRuntimeData StoneData;

	// ---- BP 绑定的组件 ----

	/** 名称 TextBlock——在 BP 里放一个叫 NameText 的 TextBlock 即可自动绑定。
	 *  InitializeEntry 时自动 SetText(DisplayName)，不用手写 BP 逻辑 */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "ClcStoneEntry|UI")
	UTextBlock* NameText;

	// ---- Tooltip 配置 ----

	/** Tooltip BP 类——在 BP 子类的 Class Defaults 里设默认值 = WBP_StoneTooltip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcStoneEntry")
	TSubclassOf<UClcStoneTooltipWidget> TooltipClass;

private:
	/** 当前显示中的 tooltip 实例 */
	UPROPERTY()
	UClcStoneTooltipWidget* TooltipRef = nullptr;

	/** 销毁当前 tooltip（如果存在） */
	void DestroyTooltip();

	/** 拿 MarketSubsystem（GameInstance 子系统） */
	UClcStoneMarketSubsystem* GetMarketSubsystem() const;
};
