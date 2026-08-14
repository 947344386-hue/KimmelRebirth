// Copyright ClaudeCore. All Rights Reserved.

#include "Actors/ClcToolUpgradeStation.h"
#include "ClcLog.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ClcInteractionIndicator.h"
#include "Components/ClcInteractionComponent.h"
#include "Subsystems/ClcBackpackSubsystem.h"
#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "Subsystems/ClcSaveManagerSubsystem.h"
#include "Subsystems/ClcKeyPromptSubsystem.h"
#include "Subsystems/ClcLogToastSubsystem.h"
#include "Quest/ClcQuestSubsystem.h"
#include "Actors/ClcFacilityManager.h"
#include "Actors/ClcStoneStall.h"
#include "UI/ClcToolUpgradeMenuWidget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AClcToolUpgradeStation::AClcToolUpgradeStation()
{
	// F 键提示与路由已由 UClcInteractionComponent 统一管理，本站无 Tick 工作
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.TickInterval = 0.0f;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	RootComponent = StationRoot;

	// 视觉 Mesh（BP 中设）—— QueryOnly 让交互组件中心球扫命中
	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(StationRoot);
	StationMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StationMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	StationMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StationMesh->SetGenerateOverlapEvents(false);

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	TriggerSphere->SetupAttachment(StationRoot);
	TriggerSphere->InitSphereRadius(InteractionRadius);
	TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// aim 模式：只有摄像机球扫命中才算选中（与修理站一致，多台不串）
	InteractionIndicator = CreateDefaultSubobject<UClcInteractionIndicator>(TEXT("InteractionIndicator"));
	InteractionIndicator->bSelectByProximity = false;
	InteractionIndicator->InteractionRadius = InteractionRadius;

	// 默认内置「手电擦石器」一项
	Upgrades.AddDefaulted();

	// 第二项：解石台
	FClcToolUpgradeItem& CuttingItem = Upgrades.AddDefaulted_GetRef();
	CuttingItem.Type = EClcToolUpgrade::CuttingTable;
	CuttingItem.Name = TEXT("解石台");
	CuttingItem.Description = TEXT("解锁解石台入口，铡刀切石一刀穷一刀富");
	CuttingItem.Cost = 3000;

	// 第三项：换批档口（可重复购买）
	FClcToolUpgradeItem& RefreshItem = Upgrades.AddDefaulted_GetRef();
	RefreshItem.Type = EClcToolUpgrade::RefreshStalls;
	RefreshItem.Name = TEXT("换批档口");
	RefreshItem.Description = TEXT("塞给暗庄一笔茶水钱，让场子里现有的摊位收拾走人，换一批带着新场口原石的摊主进谷。");
	RefreshItem.Cost = 2000;
	RefreshItem.bRepeatable = true;

	MenuWidgetClass = UClcToolUpgradeMenuWidget::StaticClass();
}

void AClcToolUpgradeStation::BeginPlay()
{
	Super::BeginPlay();

	TriggerSphere->SetSphereRadius(InteractionRadius);
	InteractionIndicator->InteractionRadius = InteractionRadius;

	// 蓝图实例的 CDO 序列化数组会覆盖构造函数默认项——运行时补增缺失项
	const bool bHasRefreshStalls = Upgrades.ContainsByPredicate(
		[](const FClcToolUpgradeItem& Item) { return Item.Type == EClcToolUpgrade::RefreshStalls; });
	if (!bHasRefreshStalls)
	{
		FClcToolUpgradeItem& RefreshItem = Upgrades.AddDefaulted_GetRef();
		RefreshItem.Type = EClcToolUpgrade::RefreshStalls;
		RefreshItem.Name = TEXT("换批档口");
		RefreshItem.Description = TEXT("塞给暗庄一笔茶水钱，让场子里现有的摊位收拾走人，换一批带着新场口原石的摊主进谷。");
		RefreshItem.Cost = 2000;
		RefreshItem.bRepeatable = true;
	}

	TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AClcToolUpgradeStation::OnTriggerBeginOverlap);
	TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AClcToolUpgradeStation::OnTriggerEndOverlap);
}

void AClcToolUpgradeStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bMenuOpen)
	{
		CloseMenu();
	}
	// F 键提示已由 UClcInteractionComponent 统一管理
	Super::EndPlay(EndPlayReason);
}

// ---- IClcInteractable ----

FText AClcToolUpgradeStation::GetInteractionPrompt() const
{
	return BuildInteractionPrompt();
}

bool AClcToolUpgradeStation::OnInteract(AActor* Interactor)
{
	if (!CanOpenMenu())
	{
		return false;
	}

	OpenMenu();
	return bMenuOpen;
}

// ---- 菜单 ----

bool AClcToolUpgradeStation::CanOpenMenu() const
{
	if (bMenuOpen || !bPlayerInRange || !CachedPC.IsValid())
	{
		return false;
	}

	const APlayerController* PC = CachedPC.Get();
	const ULocalPlayer* LP = PC->GetLocalPlayer();
	const UClcBackpackSubsystem* Backpack = LP ? LP->GetSubsystem<UClcBackpackSubsystem>() : nullptr;
	return (!Backpack || !Backpack->IsBackpackOpen())
		&& !PC->bShowMouseCursor
		&& !PC->IsMoveInputIgnored()
		&& !PC->IsLookInputIgnored();
}

void AClcToolUpgradeStation::OpenMenu()
{
	if (!CanOpenMenu()) return;

	if (!MenuWidgetClass)
	{
		MenuWidgetClass = UClcToolUpgradeMenuWidget::StaticClass();
	}

	MenuWidget = CreateWidget<UClcToolUpgradeMenuWidget>(CachedPC.Get(), MenuWidgetClass);
	if (!MenuWidget) return;

	MenuWidget->OnPurchaseRequested.RemoveDynamic(this, &AClcToolUpgradeStation::HandlePurchaseRequested);
	MenuWidget->OnPurchaseRequested.AddDynamic(this, &AClcToolUpgradeStation::HandlePurchaseRequested);
	MenuWidget->OnClosed.RemoveDynamic(this, &AClcToolUpgradeStation::HandleMenuClosed);
	MenuWidget->OnClosed.AddDynamic(this, &AClcToolUpgradeStation::HandleMenuClosed);

	MenuWidget->AddToViewport(80);
	RefreshMenuItems();

	// UI 输入模式 + 光标 + 暂停移动/视角
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(MenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	CachedPC->SetInputMode(Mode);
	CachedPC->bShowMouseCursor = true;
	CachedPC->SetIgnoreMoveInput(true);
	CachedPC->SetIgnoreLookInput(true);
	bMenuOpen = true;
	OnUpgradeMenuOpened();

	// 鼠标点击选项按钮会抢走键盘焦点，导致根 widget 的 NativeOnKeyDown（空格/方向键）不触发。
	// 打开菜单时把焦点交给根 widget，并保证后续点击选项后焦点回根（见 SelectItem）。
	MenuWidget->SetKeyboardFocus();
}

void AClcToolUpgradeStation::CloseMenu()
{
	if (MenuWidget)
	{
		MenuWidget->OnPurchaseRequested.RemoveDynamic(this, &AClcToolUpgradeStation::HandlePurchaseRequested);
		MenuWidget->OnClosed.RemoveDynamic(this, &AClcToolUpgradeStation::HandleMenuClosed);
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	if (CachedPC.IsValid())
	{
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(CachedPC.Get());
		CachedPC->bShowMouseCursor = false;
		CachedPC->SetIgnoreMoveInput(false);
		CachedPC->SetIgnoreLookInput(false);
	}

	bMenuOpen = false;
	OnUpgradeMenuClosed();
}

void AClcToolUpgradeStation::RefreshMenuItems()
{
	if (!MenuWidget) return;

	UClcBackpackSubsystem* Backpack = nullptr;
	UClcToolDurabilitySubsystem* DuraSys = nullptr;
	if (CachedPC.IsValid() && CachedPC->GetLocalPlayer())
	{
		Backpack = CachedPC->GetLocalPlayer()->GetSubsystem<UClcBackpackSubsystem>();
		DuraSys = CachedPC->GetLocalPlayer()->GetSubsystem<UClcToolDurabilitySubsystem>();
	}
	const int32 CurrentGold = Backpack ? Backpack->GetGold() : 0;

	TArray<FClcToolUpgradeItemView> Views;
	for (int32 i = 0; i < Upgrades.Num(); ++i)
	{
		const FClcToolUpgradeItem& Item = Upgrades[i];

		// 已拥有的升级不在列表里显示（可重复购买项除外）
		const bool bOwned = DuraSys ? DuraSys->OwnsUpgrade(Item.Type) : false;
		if (bOwned && !Item.bRepeatable) continue;

		FClcToolUpgradeItemView& View = Views.AddDefaulted_GetRef();
		View.Item = Item;
		View.bOwned = false;
		View.bAffordable = CurrentGold >= Item.Cost;
		View.SourceIndex = i;
	}

	MenuWidget->SetItems(Views);
}

void AClcToolUpgradeStation::ExecutePurchase(int32 ItemIndex)
{
	if (!Upgrades.IsValidIndex(ItemIndex) || !CachedPC.IsValid()) return;

	const FClcToolUpgradeItem& Item = Upgrades[ItemIndex];

	ULocalPlayer* LP = CachedPC->GetLocalPlayer();
	if (!LP) return;

	UClcBackpackSubsystem* Backpack = LP->GetSubsystem<UClcBackpackSubsystem>();
	UClcToolDurabilitySubsystem* DuraSys = LP->GetSubsystem<UClcToolDurabilitySubsystem>();
	if (!Backpack || !DuraSys) return;

	// 已拥有（可重复购买项跳过此检查）
	if (!Item.bRepeatable && DuraSys->OwnsUpgrade(Item.Type))
	{
		OnUpgradeFailed_AlreadyOwned(Item);
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(FString::Printf(TEXT("已拥有「%s」"), *Item.Name), 2.0f, FLinearColor::Yellow);
		}
		return;
	}

	// 金币不足
	const int32 CurrentGold = Backpack->GetGold();
	if (CurrentGold < Item.Cost)
	{
		OnUpgradeFailed_NotEnoughGold(Item, CurrentGold);
		if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
		{
			Toast->AddLog(FString::Printf(TEXT("金币不足！需要 %d，当前 %d"), Item.Cost, CurrentGold), 2.5f, FLinearColor::Red);
		}
		return;
	}

	// 扣金币 + 授予升级
	if (!Backpack->SpendGold(Item.Cost))
	{
		UE_LOG(LogClaudeCore, Warning, TEXT("[ClcToolUpgradeStation] SpendGold failed despite gold check passing!"));
		return;
	}
	DuraSys->GrantUpgrade(Item.Type);

	// 通知任务系统：刷新解锁升级绝对型目标（可重复购买项跳过）
	if (!Item.bRepeatable)
	{
		if (CachedPC.IsValid())
		{
			if (const ULocalPlayer* QuestLP = CachedPC->GetLocalPlayer())
			{
				if (UClcQuestSubsystem* QS = QuestLP->GetSubsystem<UClcQuestSubsystem>())
				{
					QS->NotifyObjectiveProgress(EClcQuestObjectiveType::UnlockUpgrade, 0);
				}
			}
		}
	}

	if (UClcLogToastSubsystem* Toast = ClcGetLogToast(CachedPC))
	{
		// 可重复购买项不用"获得升级"措辞
		if (Item.bRepeatable)
		{
			Toast->AddLog(FString::Printf(TEXT("%s（花费 %d 金币）"), *Item.Name, Item.Cost),
				2.5f, FLinearColor(0.2f, 1.0f, 0.4f));
		}
		else
		{
			Toast->AddLog(FString::Printf(TEXT("获得升级：%s（花费 %d 金币）"), *Item.Name, Item.Cost),
				2.5f, FLinearColor(0.2f, 1.0f, 0.4f));
		}
	}

	OnUpgradePurchased(Item);
	RefreshMenuItems();

	// 通知设施管理器刷新布局（如购买解石台升级后生成解石台）
	for (TActorIterator<AClcFacilityManager> It(GetWorld()); It; ++It)
	{
		It->RefreshLayout();
	}

	// 换批档口：先生成 → 封顶 → 存档
	if (Item.Type == EClcToolUpgrade::RefreshStalls)
	{
		for (TActorIterator<AClcStoneStall> It(GetWorld()); It; ++It)
		{
			It->RefreshAllStalls();
		}
		// 换批完成后立即存档（新批次状态）
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				if (UClcSaveManagerSubsystem* SM = GI->GetSubsystem<UClcSaveManagerSubsystem>())
				{
					SM->SaveGame(UClcSaveManagerSubsystem::AutoSaveSlotName);
				}
			}
		}
	}

	UE_LOG(LogClaudeCore, Log, TEXT("[ClcToolUpgradeStation] Upgrade purchased: %s, cost=%d"),
		*Item.Name, Item.Cost);
}

void AClcToolUpgradeStation::HandlePurchaseRequested(int32 ItemIndex)
{
	ExecutePurchase(ItemIndex);
}

void AClcToolUpgradeStation::HandleMenuClosed()
{
	CloseMenu();
}

// ---- 辅助 ----

FText AClcToolUpgradeStation::BuildInteractionPrompt() const
{
	if (!InteractionPrompt.IsEmpty())
	{
		return InteractionPrompt;
	}
	return FText::FromString(FString::Printf(TEXT("按 %s 拓局专线"), *EnterKey.ToString()));
}

// IsLookedAtByPlayer / CachedInteractionComp 已删除——F 键路由由 UClcInteractionComponent 统一接管

// ---- 蓝图事件默认实现 ----

void AClcToolUpgradeStation::OnUpgradeMenuOpened_Implementation() {}
void AClcToolUpgradeStation::OnUpgradeMenuClosed_Implementation() {}
void AClcToolUpgradeStation::OnUpgradePurchased_Implementation(const FClcToolUpgradeItem& Item) {}
void AClcToolUpgradeStation::OnUpgradeFailed_AlreadyOwned_Implementation(const FClcToolUpgradeItem& Item) {}
void AClcToolUpgradeStation::OnUpgradeFailed_NotEnoughGold_Implementation(const FClcToolUpgradeItem& Item, int32 CurrentGold) {}

// ---- 重叠 ----

void AClcToolUpgradeStation::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!Other->IsA<APawn>()) return;
	APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	bPlayerInRange = true;
	CachedPC = Cast<APlayerController>(Pawn->GetController());
}

void AClcToolUpgradeStation::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!Other->IsA<APawn>()) return;
	APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsLocallyControlled()) return;

	bPlayerInRange = false;

	// 离开范围 → 若菜单开着则关闭
	if (bMenuOpen)
	{
		CloseMenu();
	}

	// F 键提示已由 UClcInteractionComponent 统一管理
	CachedPC.Reset();
}
