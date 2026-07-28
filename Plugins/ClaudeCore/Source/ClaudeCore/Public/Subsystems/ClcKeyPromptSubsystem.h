// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "InputCoreTypes.h"
#include "ClcKeyPromptSubsystem.generated.h"

class UClcKeyPromptWidget;

USTRUCT(BlueprintType)
struct FClcKeyPrompt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ClcKeyPrompt")
	FKey Key;

	UPROPERTY(BlueprintReadWrite, Category = "ClcKeyPrompt")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "ClcKeyPrompt")
	FName Category;

	UPROPERTY(BlueprintReadWrite, Category = "ClcKeyPrompt")
	int32 SortPriority = 0;
};

/**
 * 通用左下角按键提示 HUD 后端。
 *
 * 各业务（背包/工作台/传送等）在自己进入/离开上下文时调用
 * RegisterKeyPrompt/UnregisterKeyPrompt；同 Key 引用计数去重，
 * 多个重叠上下文只显示一条。全局闸门（鼠标未显示 + Move/Look 未被忽略）
 * 决定 Widget 显隐，与传送菜单/工作台/背包的输入占用状态机一致。
 *
 * UI 层（BP）：WBP_KeyPrompt 继承 UClcKeyPromptWidget（或用 C++ 默认布局）。
 */
UCLASS()
class CLAUDECORE_API UClcKeyPromptSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 注册一条按键提示；同 Key 已存在则引用计数+1，返回新 Handle（0=无效）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcKeyPrompt")
	int32 RegisterKeyPrompt(const FKey& Key, const FText& Label, FName Category, int32 SortPriority);

	/** 注销一条按键提示；引用计数归零才移除。重复/无效静默忽略。 */
	UFUNCTION(BlueprintCallable, Category = "ClcKeyPrompt")
	void UnregisterKeyPrompt(int32 Handle);

	/** 更新已注册提示的文案（切换 CurrentVolume 等场景）。 */
	UFUNCTION(BlueprintCallable, Category = "ClcKeyPrompt")
	void UpdateKeyPromptLabel(int32 Handle, const FText& Label);

	UFUNCTION(BlueprintPure, Category = "ClcKeyPrompt")
	bool IsPromptRegistered(int32 Handle) const;

	/** 玩家是否在某个独占流程（鼠标显示或输入被占用）中，此时鹰眼等需要 3D 游戏输入的能力应禁用 */
	bool IsInExclusiveFlow() const { return !ShouldShowPrompts(); }

private:
	struct FPromptEntry
	{
		FClcKeyPrompt Prompt;
		int32 RefCount = 0;
		int32 Handle = 0;
	};

	APlayerController* GetPlayerController() const;
	bool ShouldShowPrompts() const;
	TArray<FClcKeyPrompt> GetSortedPrompts() const;
	void RebuildPromptWidget();
	void CreatePromptWidget();
	void OnGateTick();

	TArray<FPromptEntry> ActivePrompts;
	TMap<int32, FKey> HandleToKey;
	int32 NextHandle = 0;

	UPROPERTY(Transient)
	TObjectPtr<UClcKeyPromptWidget> PromptWidget;

	UPROPERTY(EditAnywhere, Category = "ClcKeyPrompt")
	TSubclassOf<UClcKeyPromptWidget> PromptWidgetClass;

	FTimerHandle GateTickHandle;
};
