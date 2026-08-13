// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Data/ClcJadeTypes.h"
#include "Data/ClcSessionTypes.h"
#include "ClcBackpackSubsystem.generated.h"

class UClcBackpackWidget;
class UClcBackpackHudWidget;

UCLASS()
class CLAUDECORE_API UClcBackpackSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- 背包/金币数据访问 ----
	TArray<FClcStoneRuntimeData> GetStones() const;
	int32 AddStone(const FClcStoneRuntimeData& StoneData);
	bool RemoveStone(int32 StoneIndex);
	int32 GetGold() const;
	void AddGold(int32 Amount);
	bool SpendGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void ToggleBackpack();

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	bool IsBackpackOpen() const { return bIsOpen; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	UClcBackpackWidget* GetBackpackWidget() const { return BackpackWidget; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	int32 GetGoldValue() const { return Gold; }

	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	int32 GetTotalEarned() const { return TotalEarned; }

	/** GM命令：增加金币（按~打开控制台输入 AddGold 50000） */
	UFUNCTION(Exec, BlueprintCallable, Category = "ClcBackpack")
	void GMAddGold(int32 Amount);

	/** 蓝图接口：增加金币 */
	UFUNCTION(BlueprintCallable, Category = "ClcBackpack")
	void AddGoldBP(int32 Amount) { AddGold(Amount); }

	/** 背包槽位上限（外部判满用，如购买前检查） */
	static constexpr int32 MAX_STONE_SLOTS = 200;

	// ---- 存档序列化 ----

	/** 从存档数据恢复背包状态 */
	void RestoreFromSaveData(const struct FClcSaveData& Data);

	/** 应用会话配置（新游戏启动时调用） */
	void SetSessionConfig(const struct FClcSessionConfig& Config);

	/** 关卡切换后 GameInstance 调用：重建常驻 HUD */
	void RebuildHud();

	/** 显/隐常驻金币条（工作台独占流程中隐藏，避免与流程 HUD 冲突） */
	void SetHudVisible(bool bVisible);

private:
	/** 金币变动后通知 SaveManager */
	void NotifySaveManagerGoldChanged();
	void ShowNotification(const FString& Message);
	void DeferredRegisterBPrompt();

	TArray<FClcStoneRuntimeData> Stones;
	int32 Gold = 0;

	UPROPERTY(Transient)
	UClcBackpackWidget* BackpackWidget;

	UPROPERTY(EditAnywhere, Category = "Backpack")
	TSubclassOf<UClcBackpackWidget> BackpackWidgetClass;

	bool bIsOpen = false;
	int32 TotalEarned = 0;

	/** 按键提示句柄：Initialize 注册 B（打开背包），Deinitialize 注销 */
	int32 BackpackPromptHandle = 0;

	// ---- 常驻金币条 HUD ----
	void DeferredCreateHud();
	/** 把当前金币/石头数量推到金币条（背包关闭时常驻显示） */
	void RefreshHud();

	UPROPERTY(Transient)
	UClcBackpackHudWidget* HudWidget;

	UPROPERTY(EditAnywhere, Category = "Backpack")
	TSubclassOf<UClcBackpackHudWidget> HudWidgetClass;

	/** 延迟创建 HUD 的重试计数（PC 未就绪时下一 tick 重试） */
	int32 HudCreateAttempts = 0;

	// ---- 打开/关闭双向滑动动画（无 FTimerHandle：靠 bSliding 标志 + SlideTarget 方向驱动 next-tick 链） ----
	/** 打开：SlideAlpha 趋向 1（上滑+淡入） */
	void StartOpenSlide();
	/** 关闭：SlideAlpha 趋向 0（下滑+淡出），到 0 由 TickSlide 自动 RemoveFromParent */
	void StartCloseSlide();
	/** 逐帧推进 SlideAlpha 到 SlideTarget，到达后停链（关闭时移除背包） */
	void TickSlide();
	/** 中断滑动链（Deinitialize 用） */
	void CancelSlide();
	/** 按 SlideAlpha 应用 RenderTransform/Opacity */
	void ApplySlideVisual();

	bool bSliding = false;
	bool bRemoveOnComplete = false;   // 关闭滑到 0 后由 TickSlide 自动 RemoveFromParent
	float SlideAlpha = 0.0f;          // 0=关闭态(下+透明) 1=打开态(就位+不透明)
	float SlideTarget = 0.0f;         // 当前目标：1=打开 0=关闭
	double LastSlideRealTime = 0.0;   // 逐帧 dt 基准
	float SlideDuration = 0.20f;      // 打开/关闭共用时长（秒）
	float SlideOffsetY = 100.0f;      // 关闭态相对就位的向下偏移（像素）
};
