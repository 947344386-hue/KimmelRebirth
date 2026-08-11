// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "ClcInteractableStation.generated.h"

class UClcBackpackSubsystem;
class UClcBackpackWidget;
class USpotLightComponent;
class UCameraComponent;
struct FClcStoneRuntimeData;

/**
 * 交互站点基类——工作台/解石台/回收商共用。
 *
 * 吸收三者逐字重复的样板：玩家引用缓存、背包引用缓存、自适应补光平滑、
 * 背包选石委托绑定、交互选中谓词、右键 FOV 放大。
 * 各子类只 override 虚钩子（UpdateFillLightTarget/IsStoneSelectable/GetAimZoomCamera/
 * OnBackpackStoneSelected）实现自己的状态机/档位/选石处理。
 *
 * 注意：RepairStation/UpgradeStation 不继承本基类——它们无补光/背包/aim-zoom，
 * 强行继承会耦合无关字段。
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Clc))
class CLAUDECORE_API AClcInteractableStation : public AActor
{
	GENERATED_BODY()

public:
	AClcInteractableStation();

protected:
	// ---- 缓存的玩家/背包引用（CachePlayerRefs 一次性填充，Tick 直接读） ----

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> CachedPC;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> PlayerInRange;

	/** 单指针缓存背包子系统（取代旧 IClcStoneCarrier 双存储 + 向下转型） */
	UPROPERTY(Transient)
	UClcBackpackSubsystem* CachedBackpack = nullptr;

	// ---- 自适应补光（组件实例在子类构造函数创建，UPROPERTY 指针在此声明供基类方法驱动） ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpotLightComponent* FillLight = nullptr;

	/** 补光强度过渡速度（越大越快，0=瞬切）。各档位强度在子类声明 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|FillLight", meta = (ClampMin = "0.0"))
	float FillLightTransitionSpeed = 10.0f;

	/** 补光当前强度（TickFillLight 每帧平滑追向 Target） */
	float CurrentFillLightIntensity = 0.0f;
	/** 补光目标强度（UpdateFillLightTarget 按状态算出） */
	float TargetFillLightIntensity = 0.0f;

	// ---- 右键 FOV 放大（仅带相机的子类用，CuttingTable 不调用） ----

	/** 长按右键放大倍率（2.0=放大2倍，只改 FOV） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|AimZoom", meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float AimZoomFactor = 2.0f;

	/** 放大过渡速度（越大越快，10≈0.1秒到位） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|AimZoom", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	float AimZoomSpeed = 10.0f;

	/** 进入模式时缓存的基础 FOV（右键放大基于此值缩放，退出时恢复） */
	float BaseFOV = 90.0f;

	// ---- 共享 concrete 方法 ----

	/** 从 PlayerInRange 解析 CachedPC + CachedBackpack（StoneVendor 干净模式） */
	void CachePlayerRefs();

	/**
	 * 绑定背包选石委托到本类的 OnBackpackStoneSelected。
	 * 子类 override OnBackpackStoneSelected 实现自己的上台/换石逻辑——
	 * UE 动态多播委托按 UFunction 反射派发，尊重虚 UFUNCTION override。
	 */
	void BindToBackpackWidget();

	/**
	 * 交互选中谓词——背包有可选石即选中态。
	 * 默认遍历 GetStones() 调 IsStoneSelectable（默认 true）。
	 * CuttingTable override IsStoneSelectable 调 IsStoneEligible。
	 */
	UFUNCTION()
	bool QueryCanSelect();

	/** 每帧把当前补光强度平滑追向目标并应用到 FillLight（三处逐字相同的实现） */
	void TickFillLight(float DeltaTime);

	/** 长按右键 FOV 放大（独立于工具，纯视觉拉近）——GetAimZoomCamera 返回 nullptr 的子类不调用 */
	void UpdateAimZoom(float DeltaTime);

	// ---- 虚钩子（子类 override） ----

	/** 按子类自己的状态枚举/档位算 TargetFillLightIntensity；默认空 */
	virtual void UpdateFillLightTarget();

	/** 单块石头是否可选上台——默认 true，CuttingTable override 调 IsStoneEligible */
	virtual bool IsStoneSelectable(const FClcStoneRuntimeData& Stone) const;

	/** 右键放大用的相机——JadeWorkbench 返回 WorkCamera，StoneVendor 返回 VendorCamera；无相机的子类返回 nullptr */
	virtual UCameraComponent* GetAimZoomCamera() const;

	/** 获取背包 Widget（CachedBackpack 非空时取其 BackpackWidget） */
	virtual UClcBackpackWidget* GetBackpackWidget() const;

	/** 背包选石回调——子类 override 实现上台/换石；基类空默认保证委托绑定安全 */
	UFUNCTION()
	virtual void OnBackpackStoneSelected(int32 StoneIndex);
};
