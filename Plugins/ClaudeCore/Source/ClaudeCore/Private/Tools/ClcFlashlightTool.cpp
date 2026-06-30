// Copyright ClaudeCore. All Rights Reserved.

#include "Tools/ClcFlashlightTool.h"
#include "Actors/ClcOpeningStone.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

AClcFlashlightTool::AClcFlashlightTool()
{
	MaxDurability = 150.0f;
	CurrentDurability = MaxDurability;

	SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
	SpotLight->SetupAttachment(ToolRoot);
	SpotLight->SetIntensity(0.0f);
	SpotLight->SetOuterConeAngle(FlashlightConeAngle);
	SpotLight->SetInnerConeAngle(0.0f);
	SpotLight->SetAttenuationRadius(FlashlightRange);
	SpotLight->SetSourceRadius(1.5f);
	SpotLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.8f));
	SpotLight->SetCastShadows(false);
	SpotLight->SetMobility(EComponentMobility::Movable);
	SpotLight->SetVisibility(false);

	// ToolMesh 挂在 SpotLight 下——在 BP 里调相对偏移和旋转
	ToolMesh->SetupAttachment(SpotLight);
}

void AClcFlashlightTool::ApplyConfig(float Intensity, float ConeAngle, float HoverHeight, float Range)
{
	FlashlightIntensity   = Intensity;
	FlashlightConeAngle   = ConeAngle;
	FlashlightHoverHeight = HoverHeight;
	FlashlightRange       = Range;

	if (SpotLight)
	{
		SpotLight->SetOuterConeAngle(ConeAngle);
		SpotLight->SetInnerConeAngle(0.0f);
		SpotLight->SetAttenuationRadius(Range);
	}
}

void AClcFlashlightTool::OnLeftClick(bool bPressed)
{
	bLightOn = bPressed && !IsBroken();

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
		}
	}
}

void AClcFlashlightTool::OnDeactivated_Implementation()
{
	// 切换工具时自动关灯
	if (bLightOn)
	{
		OnLeftClick(false);
	}
}

void AClcFlashlightTool::OnUpdate(const FClcToolTraceInfo& TraceInfo)
{
	if (!TargetStone) return;

	if (!TraceInfo.bHasHit)
	{
		bHasTarget = false;
		return;
	}

	const FVector SafeNormal = TraceInfo.SurfaceNormal.GetSafeNormal();
	const FVector LightPos = TraceInfo.HitPoint + SafeNormal * FlashlightHoverHeight;
	const FVector LightDir = -SafeNormal;

	TargetLocation = LightPos;
	TargetRotation = LightDir.Rotation();
	bHasTarget = true;

	// ─── 材质常量参数（不依赖命中点，每帧刷一次保证生效） ───
	// 注：FlashlightPos/Dir 移到 Tick，基于 SpotLight 平滑后的位姿设置，让 X-ray 光圈跟灯走
	if (UMaterialInstanceDynamic* MID = TargetStone->GetStoneMID())
	{
		const float ConeCos = FMath::Cos(FMath::DegreesToRadians(FlashlightConeAngle));
		MID->SetScalarParameterValue(TEXT("FlashlightConeCos"), ConeCos);
		MID->SetScalarParameterValue(TEXT("FlashlightRange"), FlashlightRange);
		MID->SetScalarParameterValue(TEXT("FlashlightXrayStrength"), FlashlightXrayStrength);
	}

	// ─── 耐久消耗 ───
	if (bLightOn)
	{
		ConsumeDurability(DurabilityPerSecond * GetWorld()->GetDeltaSeconds());
		if (IsBroken())
		{
			OnLeftClick(false); // 耐久耗尽自动关灯
		}
	}
}

void AClcFlashlightTool::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 平滑后的 SpotLight 位姿 → 同步材质光锥位置/方向，让 X-ray 光圈跟灯走，
	// 消除“灯慢慢移、光圈瞬间跳”的时间轴错位
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
