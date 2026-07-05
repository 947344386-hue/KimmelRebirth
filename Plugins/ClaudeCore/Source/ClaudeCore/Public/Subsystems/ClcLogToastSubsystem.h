// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "ClcLogToastSubsystem.generated.h"

class UClcLogToastListWidget;

/** log 添加时广播（UI list 绑定，创建 entry 控件） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FClcOnLogAdded, int32, LogId, const FString&, Message, const FLinearColor&, Color);

/** log 移除时广播（UI list 绑定，销毁 entry 控件） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FClcOnLogRemoved, int32, LogId);

/**
 * 通用 log 飘窗后端——任意 C++ 代码调 AddLog 即可上屏。
 *
 * 行为：
 *   - 一条 log 固定存活 Duration 秒，到时间自动移除
 *   - MaxEntries 上限，超上限 FIFO 弹掉最早的
 *   - 单 Timer 0.1s 扫描倒计时
 *   - OnLogAdded/OnLogRemoved 委托通知 UI 层
 *
 * UI 层（BP）：WBP_LogToastList 继承 UClcLogToastListWidget，自动绑委托。
 */
UCLASS()
class CLAUDECORE_API UClcLogToastSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 添加一条 log 到飘窗。
	 * @param Message  文本内容
	 * @param Duration 存活秒数（默认 2s）
	 * @param Color    可选颜色（传给 entry 控件，BP 决定怎么用）
	 * @return LogId（可用于手动 RemoveLog）
	 */
	UFUNCTION(BlueprintCallable, Category = "ClcLogToast")
	int32 AddLog(const FString& Message, float Duration = 2.0f, FLinearColor Color = FLinearColor::White);

	/** 手动移除一条 log（比如用户做了关闭按钮） */
	UFUNCTION(BlueprintCallable, Category = "ClcLogToast")
	bool RemoveLog(int32 LogId);

	// ---- 委托（UI list 绑定） ----

	UPROPERTY(BlueprintAssignable, Category = "ClcLogToast")
	FClcOnLogAdded OnLogAdded;

	UPROPERTY(BlueprintAssignable, Category = "ClcLogToast")
	FClcOnLogRemoved OnLogRemoved;

protected:
	/** list widget 类——未配置时按约定路径加载 /Game/JadeBetting/UI/WBP_LogToastList */
	UPROPERTY(EditAnywhere, Category = "ClcLogToast")
	TSubclassOf<UClcLogToastListWidget> ListWidgetClass;

	/** 同时存在的最大 log 条数（默认 5，超出 FIFO 弹最早的） */
	UPROPERTY(EditAnywhere, Category = "ClcLogToast")
	int32 MaxEntries = 5;

private:
	struct FLogEntry
	{
		int32 Id = -1;
		FString Message;
		FLinearColor Color;
		float RemainingTime = 0.0f;
	};

	TArray<FLogEntry> Entries;
	int32 NextLogId = 0;

	UPROPERTY()
	UClcLogToastListWidget* ListWidget = nullptr;

	FTimerHandle TickHandle;

	/** Timer 回调：倒计时 + 移除过期 */
	void OnTick();

	/** 创建 list widget 并上屏 */
	void CreateListWidget();

	/** 内部移除：从 Entries 删 + 广播 OnLogRemoved */
	void RemoveLogInternal(int32 LogId);
};
