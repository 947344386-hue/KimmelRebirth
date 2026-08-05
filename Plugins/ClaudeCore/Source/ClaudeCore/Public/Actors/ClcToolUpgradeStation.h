// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Tools/ClcStoneTool.h"
#include "ClcToolUpgradeStation.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UClcInteractionIndicator;
class UClcToolUpgradeMenuWidget;

/**
 * 工具升级站 —— 可放置的通用商店 Actor。
 *
 * 玩家走进范围、瞄准本站按 F → 打开升级列表菜单；选中一项花钱购买即获得对应升级。
 * 默认内置「手电开窗器」一项；BP 可在 Upgrades 数组追加更多升级项。
 *
 * 蓝图使用：
 *   1. Content 中创建继承此类的 BP（如 BP_ToolUpgradeStation）
 *   2. 设 StationMesh（有碰撞的 StaticMesh，让交互组件球扫命中）
 *   3. 按需调整 Upgrades 数组（名称/描述/价格）
 *   4. 拖入关卡
 */
UCLASS(Blueprintable, ClassGroup = (Clc))
class CLAUDECORE_API AClcToolUpgradeStation : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcToolUpgradeStation();

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 组件 ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* StationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* TriggerSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UClcInteractionIndicator* InteractionIndicator;

	// ---- 配置 ----

	/** 本站出售的升级项（默认含「手电开窗器」，BP 可追加） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpgradeStation|Config")
	TArray<FClcToolUpgradeItem> Upgrades;

	/** 交互触发距离 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpgradeStation|Config", meta = (ClampMin = "50.0"))
	float InteractionRadius = 300.0f;

	/** 交互键 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpgradeStation|Config")
	FKey EnterKey = FKey("F");

	/** 交互提示文本（留空则自动生成） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpgradeStation|Config")
	FText InteractionPrompt;

	/** 升级菜单 Widget 类（默认 C++ 类，可换 BP 皮） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UpgradeStation|UI")
	TSubclassOf<UClcToolUpgradeMenuWidget> MenuWidgetClass;

	// ---- 蓝图事件 ----

	UFUNCTION(BlueprintNativeEvent, Category = "UpgradeStation|Events")
	void OnUpgradeMenuOpened();
	UFUNCTION(BlueprintNativeEvent, Category = "UpgradeStation|Events")
	void OnUpgradeMenuClosed();
	UFUNCTION(BlueprintNativeEvent, Category = "UpgradeStation|Events")
	void OnUpgradePurchased(const FClcToolUpgradeItem& Item);
	UFUNCTION(BlueprintNativeEvent, Category = "UpgradeStation|Events")
	void OnUpgradeFailed_AlreadyOwned(const FClcToolUpgradeItem& Item);
	UFUNCTION(BlueprintNativeEvent, Category = "UpgradeStation|Events")
	void OnUpgradeFailed_NotEnoughGold(const FClcToolUpgradeItem& Item, int32 CurrentGold);

private:
	/** 打开升级菜单 */
	void OpenMenu();
	/** 关闭升级菜单（恢复游戏输入） */
	void CloseMenu();
	/** 执行购买：校验→扣金币→授予升级→刷新菜单 */
	void ExecutePurchase(int32 ItemIndex);
	/** 用当前金币/所有权重建菜单商品视图 */
	void RefreshMenuItems();
	/** 生成交互提示 */
	FText BuildInteractionPrompt() const;
	/** 交互组件中心球扫是否命中本站 */
	bool IsLookedAtByPlayer() const;

	UFUNCTION()
	void HandlePurchaseRequested(int32 ItemIndex);
	UFUNCTION()
	void HandleMenuClosed();

	// ---- 重叠 ----

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ---- 运行时 ----

	UPROPERTY(Transient)
	UClcToolUpgradeMenuWidget* MenuWidget = nullptr;

	bool bMenuOpen = false;

	/** 按键边沿检测（自维护） */
	bool bEnterKeyPrev = false;
	bool bPlayerInRange = false;

	TWeakObjectPtr<APlayerController> CachedPC;

	/** 按键提示句柄 */
	int32 UpgradePromptHandle = 0;
};
