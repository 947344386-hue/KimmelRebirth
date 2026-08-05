// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcCombinedTool.h"
#include "Actors/ClcOpeningStone.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AClcCombinedTool::AClcCombinedTool()
{
	MaxDurability = 300.0f;
	CurrentDurability = MaxDurability;

	SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	SpotLight->SetupAttachment(ToolRoot);
	SpotLight->SetIntensity(0.0f);
	SpotLight->SetSourceRadius(1.5f);
	SpotLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.8f));
	SpotLight->SetCastShadows(false);
	SpotLight->SetMobility(EComponentMobility::Movable);
	SpotLight->SetVisibility(false);
	// 照搬手电筒的挂载关系：ToolMesh 挂在 SpotLight 下面，旋转天生就穿入表面
	ToolMesh->SetupAttachment(SpotLight);
	// 用 UPROPERTY 默认值填位置/锥角/范围（BP 改 UPROPERTY 后由 OnActivated 再次同步）
	RefreshSpotLightConfig();
}

void AClcCombinedTool::RefreshSpotLightConfig()
{
	if (!SpotLight) return;
	// ToolRoot +Z = 表面法线（朝外）；SpotLight 沿 +Z 抬高悬浮，旋转 -90 pitch 让光锥穿入表面
	SpotLight->SetRelativeLocation(FVector(0.0f, 0.0f, FlashlightHoverHeight));
	SpotLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	SpotLight->SetInnerConeAngle(0.0f);
	SpotLight->SetOuterConeAngle(FlashlightConeAngle);
	SpotLight->SetAttenuationRadius(FlashlightRange);
}

void AClcCombinedTool::OnActivated_Implementation()
{
	// Spawn 进工作台后，把 BP 覆写的 UPROPERTY 同步到 SpotLight（构造时只有 CDO 默认值）
	RefreshSpotLightConfig();
}

void AClcCombinedTool::ApplyLightState()
{
	if (SpotLight)
	{
		SpotLight->SetVisibility(bLightOn);
		SpotLight->SetIntensity(bLightOn ? FlashlightIntensity : 0.0f);
	}

	if (TargetStone)
	{
		if (UMaterialInstanceDynamic* MID = TargetStone->GetStoneMID())
		{
			MID->SetScalarParameterValue(TEXT("FlashlightOn"), bLightOn ? 1.0f : 0.0f);
			if (bLightOn)
			{
				const float ConeCos = FMath::Cos(FMath::DegreesToRadians(FlashlightConeAngle));
				MID->SetScalarParameterValue(TEXT("FlashlightConeCos"), ConeCos);
				MID->SetScalarParameterValue(TEXT("FlashlightRange"), FlashlightRange);
				MID->SetScalarParameterValue(TEXT("FlashlightXrayStrength"), FlashlightXrayStrength);
			}
		}
	}
}

void AClcCombinedTool::SetLightOn(bool bOn)
{
	bLightOn = bOn && !IsBroken();
	ApplyLightState();
}

void AClcCombinedTool::ToggleLight()
{
	if (bLightOn)
	{
		SetLightOn(false);
	}
	else
	{
		// 耐久耗尽时无法开灯
		SetLightOn(!IsBroken());
	}
}

void AClcCombinedTool::OnUpdate(const FClcToolTraceInfo& TraceInfo)
{
	// 先走开窗器逻辑（定位 + 打磨 + 打磨耐久消耗 + 进入时耐久提示）
	Super::OnUpdate(TraceInfo);

	// 进入工作台时若耐久已耗尽（持久化值），Super 已飘过一次提示，这里无需重复

	// 开灯且命中石头 → 消耗手电耐久（与打磨共用同一耐久池）
	if (bLightOn && TraceInfo.bHasHit)
	{
		if (UWorld* World = GetWorld())
		{
			ConsumeDurability(FlashlightDurabilityPerSecond * World->GetDeltaSeconds());
		}
		if (IsBroken())
		{
			SetLightOn(false); // 耐久耗尽自动关灯
		}
	}
}

void AClcCombinedTool::Tick(float DeltaTime)
{
	// 开窗器位姿平滑（Super=AClcOpeningTool → AClcStoneTool::Tick）
	Super::Tick(DeltaTime);

	// 把平滑后的 SpotLight 世界位姿同步到材质，让 X-ray 光圈跟灯走
	if (!bLightOn || !TargetStone || !SpotLight) return;

	UMaterialInstanceDynamic* MID = TargetStone->GetStoneMID();
	if (!MID) return;

	const FVector LightPos = SpotLight->GetComponentLocation();
	const FVector LightDir = SpotLight->GetForwardVector();
	MID->SetVectorParameterValue(TEXT("FlashlightPos"),
		FLinearColor((float)LightPos.X, (float)LightPos.Y, (float)LightPos.Z, 0.0f));
	MID->SetVectorParameterValue(TEXT("FlashlightDir"),
		FLinearColor((float)LightDir.X, (float)LightDir.Y, (float)LightDir.Z, 0.0f));
}

void AClcCombinedTool::OnDeactivated_Implementation()
{
	// 切出/退出工作台时自动关灯
	if (bLightOn)
	{
		SetLightOn(false);
	}
}
