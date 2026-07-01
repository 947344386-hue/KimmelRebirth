// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ClcInteractable.h"
#include "Data/ClcJadeTypes.h"
#include "ClcStone.generated.h"

class UStaticMeshComponent;
class UClcInteractionIndicator;

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

	/** 获取石头数据 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	const FClcStoneRuntimeData& GetStoneData() const { return RuntimeData; }

	/** 设置石头表面积（创建Mesh后由摊位调用） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void RecalculateSurfaceArea();

	// ---- IClcInteractable ----
	virtual FText GetInteractionPrompt() const override;
	virtual bool OnInteract(AActor* Interactor) override;

	/** 购买——蓝图调用此节点（包装 OnInteract） */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	bool PurchaseStone(AActor* Buyer);

	/** 在此石头上显示信息卡片 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void ShowInfoCard();

	/** 隐��信息卡片 */
	UFUNCTION(BlueprintCallable, Category = "ClcStone")
	void HideInfoCard();

	/** 从摊位场景中移除（被购买或市场刷新） */
	void RemoveFromStall();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStone")
	UStaticMeshComponent* StoneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ClcStone")
	UClcInteractionIndicator* InteractionIndicator;

	/** 摊位展示用皮壳材质路径（纯皮壳，不含开窗逻辑） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ClcStone")
	FString ShellMaterialPath = TEXT("/Game/JadeBetting/Materials/M_StoneShell.M_StoneShell");

private:
	FClcStoneRuntimeData RuntimeData;
	bool bPlayerInRange = false;
	bool bCameraAiming = false;
	bool bInfoCardVisible = false;
	float RangeCheckTimer = 0.0f;

	/** 信息卡片Widget（创建后缓存） */
	UPROPERTY()
	class UClcStoneInfoWidget* InfoCardWidget;

	UPROPERTY(EditAnywhere, Category = "ClcStone")
	TSubclassOf<UClcStoneInfoWidget> InfoCardClass;
};
