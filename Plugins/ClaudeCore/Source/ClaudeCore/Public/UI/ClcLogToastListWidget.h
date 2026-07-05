// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "ClcLogToastListWidget.generated.h"

class UClcLogToastSubsystem;

/**
 * log 飘窗 list 容器基类——C++ 自动绑委托 + 管 Id→Widget 映射 + 管 VerticalBox 插入/移除。
 *
 * BP 子类（WBP_LogToastList）只需：
 *   1. 加一个 VerticalBox，命名必须叫 "LogContainer"（BindWidget 自动绑定）
 *   2. 实现 "Create Entry Widget" 事件：Create WBP_LogEntry → 设 Message/Color → Return
 *
 * 新 log 插入顶部（InsertChildAt 0），老 log 往下顶。
 */
UCLASS(Abstract)
class CLAUDECORE_API UClcLogToastListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	/** 容器——BP 子类里放一个 VerticalBox 命名 "LogContainer"，BindWidget 自动绑定 */
	UPROPERTY(BlueprintReadOnly, Category = "ClcLogToast", meta = (BindWidget))
	UVerticalBox* LogContainer;

	/**
	 * BP 实现：创建一条 entry widget。
	 * BP 里 Create WBP_LogEntry → SetMessage/SetColor → Return。
	 * 返回的 widget 会被 C++ 自动 InsertChildAt(0) 加到 LogContainer 顶部。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "ClcLogToast", meta = (DisplayName = "Create Entry Widget"))
	UUserWidget* CreateEntryWidget(const FString& Message, const FLinearColor& Color);

private:
	UFUNCTION()
	void HandleLogAdded(int32 LogId, const FString& Message, const FLinearColor& Color);

	UFUNCTION()
	void HandleLogRemoved(int32 LogId);

	/** LogId → EntryWidget 映射（移除时定位用） */
	UPROPERTY()
	TMap<int32, UUserWidget*> EntryMap;
};
