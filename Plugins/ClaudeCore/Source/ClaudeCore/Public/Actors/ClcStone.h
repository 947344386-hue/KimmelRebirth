// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStone.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UClcInteractionIndicator;
class AClcStoneStall;

/**
 * 单块原石——摊位展示用，可被购买
 * 自带交互指示器，摄像机瞄准时显示信息卡片
 */
UCLASS()
class CLAUDECORE_API AClcStone : public AActor, public IClcInteractable
{
	GENERATED_BODY()

public:
	AClcStone();

	/** 用指定数据和模型初始化石头 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void Initialize(const FClcStoneInternalData& InData, UStaticMesh* InMesh, float InScale, const FString& InDisplayName);

	/** 获取皮壳名称（从 DA_ShellTextureConfig 查） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	FName GetShellName() const;

	/** 设置展示名 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void SetDisplayName(const FString& NewName) { RuntimeData.DisplayName = NewName; }

	/** 获取石头数据（可写引用——供 RestoreFromSlots 直接写回 RuntimeData） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	const FClcStoneRuntimeData& GetStoneData() const { return RuntimeData; }
	FClcStoneRuntimeData& GetStoneData() { return RuntimeData; }

	/** 设置石头表面积（创建Mesh后由摊位调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void RecalculateSurfaceArea();

	/** 用当前 Internal 数据重算 TheoreticalValue + PurchasePrice（供 ClcStoneStall 价格封顶等外部调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void RecalculatePrices();

	/** 缩放变动后重新贴地：底部对齐桌面顶面（传入桌面顶面世界 Z 坐标） */
	void SnapToSurface(float TableTopZ);

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

	/** 购买——蓝图调用此节点（包装 OnInteract） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	bool PurchaseStone(AActor* Buyer);

	/** 摊位生成时调用——按 DA_StallConfig 设置交互半径和瞄准球扫半径 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void ApplyInteractionConfig(float InInteractionRadius, float InAimSweepRadius);

	/** 在此石头上显示信息卡片 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void ShowInfoCard();

	/** 隐��信息卡片 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void HideInfoCard();

	/** 从摊位场景中移除（被购买或市场刷新） */
	void RemoveFromStall();

	/** 设置所属摊位——购买时通知摊位用（实现放 .cpp，避免 AClcStoneStall 不完整时 TWeakObjectPtr 赋值实例化失败） */
	void SetOwningStall(AClcStoneStall* Stall);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStone")
	UStaticMeshComponent* StoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStone")
	UClcInteractionIndicator* InteractionIndicator;

	/** 摊位展示用皮壳材质路径（纯皮壳，不含擦石逻辑） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcStone")
	FString ShellMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneShell.M_StoneShell");

private:
	FClcStoneRuntimeData RuntimeData;
	bool bCameraAiming = false;
	bool bInfoCardVisible = false;
	float RangeCheckTimer = 0.0f;

	/** 所属摊位——购买时通知摊位移除 + 广播给商人 */
	UPROPERTY()
	TWeakObjectPtr<AClcStoneStall> OwningStall;

	/** 信息卡片Widget（创建后缓存） */
	UPROPERTY()
	class UClcStoneInfoWidget* InfoCardWidget;

	UPROPERTY(EditAnywhere, Category = "ClcStone")
	TSubclassOf<UClcStoneInfoWidget> InfoCardClass;
};
