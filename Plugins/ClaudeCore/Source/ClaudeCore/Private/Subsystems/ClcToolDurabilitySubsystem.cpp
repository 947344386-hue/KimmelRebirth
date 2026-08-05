// Copyright ClaudeCore. All Rights Reserved.

#include "Subsystems/ClcToolDurabilitySubsystem.h"
#include "ClcLog.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

void UClcToolDurabilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

UClcToolDurabilitySubsystem* UClcToolDurabilitySubsystem::Get(const UWorld* World)
{
	if (!World) return nullptr;

	if (const UGameInstance* GI = World->GetGameInstance())
	{
		if (const ULocalPlayer* LP = GI->GetFirstGamePlayer())
		{
			return LP->GetSubsystem<UClcToolDurabilitySubsystem>();
		}
	}
	return nullptr;
}

void UClcToolDurabilitySubsystem::InitTool(EClcRepairableTool ToolType, float MaxDurability)
{
	if (ToolType == EClcRepairableTool::None) return;

	MaxDurabilityStore.Add(ToolType, MaxDurability);

	// 尚无存储耐久 → 用 max 初始化（首次进入为满耐久）
	if (!DurabilityStore.Contains(ToolType))
	{
		DurabilityStore.Add(ToolType, MaxDurability);
	}
}

float UClcToolDurabilitySubsystem::GetDurability(EClcRepairableTool ToolType) const
{
	if (const float* Found = DurabilityStore.Find(ToolType))
	{
		return FMath::Clamp(*Found, 0.0f, GetMaxDurability(ToolType));
	}
	// 未存储过 → 返回注册的最大耐久（或兜底）
	return GetMaxDurability(ToolType);
}

void UClcToolDurabilitySubsystem::SetDurability(EClcRepairableTool ToolType, float Value)
{
	if (ToolType == EClcRepairableTool::None) return;
	const float Clamped = FMath::Clamp(Value, 0.0f, GetMaxDurability(ToolType));
	DurabilityStore.Add(ToolType, Clamped);
}

void UClcToolDurabilitySubsystem::RestoreDurability(EClcRepairableTool ToolType)
{
	if (ToolType == EClcRepairableTool::None) return;
	const float MaxVal = GetMaxDurability(ToolType);
	DurabilityStore.Add(ToolType, MaxVal);
}

void UClcToolDurabilitySubsystem::RestoreDurabilityMask(int32 Mask)
{
	if (Mask & static_cast<int32>(EClcRepairableTool::Opener))
	{
		RestoreDurability(EClcRepairableTool::Opener);
	}
	if (Mask & static_cast<int32>(EClcRepairableTool::Flashlight))
	{
		RestoreDurability(EClcRepairableTool::Flashlight);
	}
	if (Mask & static_cast<int32>(EClcRepairableTool::Combined))
	{
		RestoreDurability(EClcRepairableTool::Combined);
	}
}

bool UClcToolDurabilitySubsystem::OwnsUpgrade(EClcToolUpgrade Upgrade) const
{
	return OwnedUpgrades.Contains(Upgrade);
}

bool UClcToolDurabilitySubsystem::GrantUpgrade(EClcToolUpgrade Upgrade)
{
	if (OwnedUpgrades.Contains(Upgrade))
	{
		return false;
	}
	OwnedUpgrades.Add(Upgrade);
	return true;
}

bool UClcToolDurabilitySubsystem::NeedsRepair(EClcRepairableTool ToolType) const
{
	return GetDurability(ToolType) < GetMaxDurability(ToolType) - 0.01f;
}

float UClcToolDurabilitySubsystem::GetMaxDurability(EClcRepairableTool ToolType) const
{
	if (const float* Found = MaxDurabilityStore.Find(ToolType))
	{
		return *Found;
	}
	// 未注册 → 兜底（不应发生，工具 Spawn 时会 InitTool）
	return 100.0f;
}

float UClcToolDurabilitySubsystem::GetDurabilityRatio(EClcRepairableTool ToolType) const
{
	const float Max = GetMaxDurability(ToolType);
	return Max > 0.0f ? GetDurability(ToolType) / Max : 0.0f;
}