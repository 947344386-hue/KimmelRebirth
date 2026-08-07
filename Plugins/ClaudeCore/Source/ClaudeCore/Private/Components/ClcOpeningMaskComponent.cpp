// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcOpeningMaskComponent.h"
#include "ClcLog.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"

UClcOpeningMaskComponent::UClcOpeningMaskComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UClcOpeningMaskComponent::BeginDestroy()
{
	// 清理 GC Root——AddToRoot 的纹理必须显式 RemoveFromRoot，否则永不回收
	if (RevealTex)
	{
		RevealTex->RemoveFromRoot();
		RevealTex = nullptr;
	}
	if (TypeTex)
	{
		TypeTex->RemoveFromRoot();
		TypeTex = nullptr;
	}
	// MaskRT 是 UPROPERTY，GC 会自动回收，不需要 RemoveFromRoot
	Super::BeginDestroy();
}

// ============================================================
// 初始化
// ============================================================

void UClcOpeningMaskComponent::InitializeFromStoneData(const FClcStoneInternalData& StoneData)
{
	EnsureMaskRT();
	EnsureRevealTexFromDistribution(StoneData.DistributionMap, StoneData.Seed, StoneData.Grade);
	EnsureTypeTexFromDistribution(StoneData.DistributionMap);
	ResetMask();

	CachedDistribution = StoneData.DistributionMap;
	CachedSeed = StoneData.Seed;
	CachedGrade = StoneData.Grade;
}

void UClcOpeningMaskComponent::ResetMask()
{
	const int32 TotalPixels = MaskResolution * MaskResolution;
	MaskBuffer.Init(0, TotalPixels);
	OpenedPixelCount = 0;
	OpenedGreenPixelCount = 0;
	OpenedImpurityPixelCount = 0;
	OpenedCrackPixelCount = 0;
	UploadMaskToGPU();
}

void UClcOpeningMaskComponent::SaveMaskToData(FClcStoneRuntimeData& OutData) const
{
	OutData.SavedMaskBuffer = MaskBuffer;
}

void UClcOpeningMaskComponent::RestoreMaskFromData(const FClcStoneRuntimeData& InData)
{
	if (InData.SavedMaskBuffer.Num() == MaskResolution * MaskResolution)
	{
		MaskBuffer = InData.SavedMaskBuffer;
		// 从存档重算已擦石像素数 + 玉/杂质/裂暴露量
		OpenedPixelCount = 0;
		OpenedGreenPixelCount = 0;
		OpenedImpurityPixelCount = 0;
		OpenedCrackPixelCount = 0;
		for (int32 Idx = 0; Idx < MaskBuffer.Num(); ++Idx)
		{
			if (MaskBuffer[Idx] >= 128)
			{
				++OpenedPixelCount;
				const int32 X = Idx % MaskResolution;
				const int32 Y = Idx / MaskResolution;
				const uint8 MatType = CachedDistribution.GetPixel(X, Y);
				if (MatType == JadeBody) ++OpenedGreenPixelCount;
				else if (MatType == Impurity) ++OpenedImpurityPixelCount;
				else if (MatType == Crack) ++OpenedCrackPixelCount;
			}
		}
		EnsureMaskRT();
		UploadMaskToGPU();
		ensure(RevealTex || CachedDistribution.Data.Num() > 0);
	}
	else
	{
		ResetMask();
	}
}

void UClcOpeningMaskComponent::EnsureMaskRT()
{
	if (MaskRT) return;

	MaskRT = NewObject<UTextureRenderTarget2D>(this, TEXT("OpeningMaskRT"));
	MaskRT->InitCustomFormat(MaskResolution, MaskResolution, PF_G8, false);
	MaskRT->ClearColor = FLinearColor::Black;
	MaskRT->UpdateResourceImmediate(true);

	// 初始 CPU 缓冲区
	const int32 TotalPixels = MaskResolution * MaskResolution;
	MaskBuffer.Init(0, TotalPixels);
}

void UClcOpeningMaskComponent::EnsureRevealTexFromDistribution(const FClcStoneDistributionMap& Distribution, int32 Seed, EClcJadeGrade Grade)
{
	if (RevealTex)
	{
		RevealTex->RemoveFromRoot();
		RevealTex = nullptr;
	}

	const int32 Res = Distribution.Resolution;
	const int32 TotalPixels = Res * Res;

	TArray<FColor> Pixels;
	Pixels.SetNum(TotalPixels);

	// 确定性多层噪声——每种水有自己的调色板和纹理特征
	FRandomStream Rng(Seed * 987654321);

	// Multi-octave noise layers (deterministic from Seed)
	TArray<int32> OctaveSeeds;
	for (int32 o = 0; o < 5; ++o) { OctaveSeeds.Add(Seed * 137 + o * 997); }

	// Pre-generate noise fields at various frequencies
	TArray<TArray<float>> NoiseLayers;
	NoiseLayers.SetNum(5);
	for (int32 o = 0; o < 5; ++o)
	{
		NoiseLayers[o].SetNum(TotalPixels);
		const int32 Freq = 3 * (1 << o);
		FRandomStream NS(OctaveSeeds[o]);
		for (int32 Y = 0; Y < Res; ++Y)
		{
			const float YPhase = NS.FRand() * 6.28318f;
			for (int32 X = 0; X < Res; ++X)
			{
				const float XPhase = NS.FRand() * 6.28318f;
				float Acc = 0.0f;
				for (int32 w = 0; w < Freq; ++w)
				{
					Acc += FMath::Sin(static_cast<float>(X) / static_cast<float>(Res) * 6.28318f * Freq + XPhase + w * 0.7f) *
					       FMath::Sin(static_cast<float>(Y) / static_cast<float>(Res) * 6.28318f * Freq + YPhase + w * 1.1f);
				}
				NoiseLayers[o][Y * Res + X] = Acc / static_cast<float>(Freq);
			}
		}
	}

	// 组装多频噪声：高频细节叠到低频基底上
	auto SampleNoise = [&](int32 X, int32 Y) -> float
	{
		const int32 Idx = Y * Res + X;
		float v = 0.0f;
		for (int32 o = 0; o < 5; ++o)
		{
			v += NoiseLayers[o][Idx] / static_cast<float>(1 << (o + 1));
		}
		return FMath::Clamp(v * 0.5f + 0.5f, 0.0f, 1.0f);
	};

	// ── 种水调色板 ──
	struct FJadePalette { uint8 R, G, B; };
	FJadePalette GreenBase, GreenDark, GreenBright;
	int32 JadeGrainStrength = 40;    // 纹理颗粒强度
	int32 JadeVeinStrength = 30;     // 色根/絮状纹理强度
	int32 CrackIntensity = 25;       // 杂裂黑度范围

	switch (Grade)
	{
	default:
	case EClcJadeGrade::Bean:  // 豆种—暗暖绿，不透明，噪点多
		GreenBase   = { 30, 140, 50 };
		GreenDark   = { 15, 100, 30 };
		GreenBright = { 60, 180, 90 };
		JadeGrainStrength = 55;
		JadeVeinStrength = 40;
		CrackIntensity = 30;
		break;
	case EClcJadeGrade::Glutinous: // 糯种—暖奶绿，絮状纹理，半透
		GreenBase   = { 50, 160, 80 };
		GreenDark   = { 30, 120, 50 };
		GreenBright = { 90, 210, 130 };
		JadeGrainStrength = 40;
		JadeVeinStrength = 50;
		CrackIntensity = 22;
		break;
	case EClcJadeGrade::Ice: // 冰种—冷翠绿，冰裂细纹，高透
		GreenBase   = { 40, 175, 110 };
		GreenDark   = { 20, 135, 70 };
		GreenBright = { 80, 225, 170 };
		JadeGrainStrength = 25;
		JadeVeinStrength = 35;
		CrackIntensity = 18;
		break;
	case EClcJadeGrade::Glass: // 玻璃种—鲜艳翠绿，荧光感，几乎无杂质
		GreenBase   = { 45, 200, 100 };
		GreenDark   = { 30, 160, 60 };
		GreenBright = { 100, 240, 150 };
		JadeGrainStrength = 15;
		JadeVeinStrength = 20;
		CrackIntensity = 10;
		break;
	}

	FRandomStream CrackRng(Seed * 777);

	for (int32 Y = 0; Y < Res; ++Y)
	{
		for (int32 X = 0; X < Res; ++X)
		{
			const uint8 Val = Distribution.GetPixel(X, Y);
			FColor& Pixel = Pixels[Y * Res + X];

			if (Val == JadeBody) // 玉肉
			{
				const float Noise = SampleNoise(X, Y);
				const float MicroNoise = (CrackRng.FRand() - 0.5f) * 0.15f;

				// 颜色在 Base / Dark / Bright 间按噪声插值
				const float T1 = FMath::Clamp(Noise * 1.2f - 0.1f, 0.0f, 1.0f);
				const float Vein = FMath::Abs(FMath::Sin(Noise * 12.0f + X * 0.07f + Y * 0.11f));
				const float VeinMask = FMath::Clamp(Vein * 2.5f, 0.0f, 1.0f);

				int32 R = FMath::Lerp(GreenDark.R, GreenBase.R, T1);
				int32 G = FMath::Lerp(GreenDark.G, GreenBase.G, T1);
				int32 B = FMath::Lerp(GreenDark.B, GreenBase.B, T1);

				// 亮色斑块
				if (Noise > 0.65f)
				{
					const float BT = (Noise - 0.65f) / 0.35f;
					R = FMath::Lerp(R, GreenBright.R, BT * 0.7f);
					G = FMath::Lerp(G, GreenBright.G, BT * 0.7f);
					B = FMath::Lerp(B, GreenBright.B, BT * 0.7f);
				}

				// 絮状色根（暗纹）
				const float VeinStrength = VeinMask * static_cast<float>(JadeVeinStrength) / 100.0f;
				R -= FMath::RoundToInt(VeinStrength * 18);
				G -= FMath::RoundToInt(VeinStrength * 22);
				B -= FMath::RoundToInt(VeinStrength * 10);

				// 颗粒噪点
				const float Grain = static_cast<float>(JadeGrainStrength) * (0.5f + MicroNoise);
				R += FMath::RoundToInt(Grain * (CrackRng.FRand() - 0.5f));
				G += FMath::RoundToInt(Grain * (CrackRng.FRand() - 0.5f));
				B += FMath::RoundToInt(Grain * (CrackRng.FRand() - 0.5f));

				Pixel.R = static_cast<uint8>(FMath::Clamp(R, 0, 255));
				Pixel.G = static_cast<uint8>(FMath::Clamp(G, 0, 255));
				Pixel.B = static_cast<uint8>(FMath::Clamp(B, 0, 255));
				Pixel.A = 255;
			}
			else // 杂裂(2)
			{
				const float Noise = SampleNoise(X, Y);
				const float CrackNoise = FMath::Abs(FMath::Sin(Noise * 25.0f + CrackRng.FRand() * 0.3f));

				// 深色裂纹：不是纯黑，而是深灰到黑的变化
				const int32 DarkBase = 8 + FMath::RoundToInt(Noise * static_cast<float>(CrackIntensity));
				const int32 CrackBias = FMath::RoundToInt(CrackNoise * 12);

				int32 R = DarkBase + CrackBias;
				int32 G = DarkBase + CrackBias - 2;
				int32 B = DarkBase + CrackBias - 3;

				// 微弱的矿脉穿插（偶尔有偏红/棕的深色纹路）
				if (CrackNoise > 0.7f)
				{
					R += FMath::RoundToInt((CrackNoise - 0.7f) * 40);
				}

				Pixel.R = static_cast<uint8>(FMath::Clamp(R, 3, 60));
				Pixel.G = static_cast<uint8>(FMath::Clamp(G, 2, 55));
				Pixel.B = static_cast<uint8>(FMath::Clamp(B, 0, 50));
				Pixel.A = 255;
			}
		}
	}

	// 创建 Texture2D
	RevealTex = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
	if (!RevealTex) return;

	FTexturePlatformData* PlatformData = RevealTex->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0) return;
	void* RawData = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(RawData, Pixels.GetData(), TotalPixels * sizeof(FColor));
	PlatformData->Mips[0].BulkData.Unlock();

	RevealTex->SRGB = true;
	RevealTex->CompressionSettings = TC_Default;
#if WITH_EDITORONLY_DATA
	RevealTex->MipGenSettings = TMGS_NoMipmaps;
#endif
	RevealTex->UpdateResource();

	RevealTex->AddToRoot();
}

void UClcOpeningMaskComponent::EnsureTypeTexFromDistribution(const FClcStoneDistributionMap& Distribution)
{
	if (TypeTex)
	{
		TypeTex->RemoveFromRoot();
		TypeTex = nullptr;
	}

	const int32 Res = Distribution.Resolution;
	const int32 TotalPixels = Res * Res;

	// R 通道 = 玉mask(255=玉), G 通道 = 杂mask(255=杂)
	TArray<FColor> Pixels;
	Pixels.SetNum(TotalPixels);

	for (int32 Y = 0; Y < Res; ++Y)
	{
		for (int32 X = 0; X < Res; ++X)
		{
			const uint8 Val = Distribution.GetPixel(X, Y);
			FColor& Pixel = Pixels[Y * Res + X];

			// 材质 M_StoneOpening 按 R=玉 / G=杂 双通道混合 Jade/Junk PBR。
			// 玉肉走 Jade；杂质/裂纹/废肉均走 Junk，靠“小簇 vs 细线 vs 大片”几何形态区分视觉。
			if (Val == JadeBody)
			{
				Pixel = FColor(255, 0, 0, 255);   // R=255（玉）
			}
			else // Impurity / Crack / HostWaste → Junk
			{
				Pixel = FColor(0, 255, 0, 255);   // G=255（杂）
			}
		}
	}

	// ---- 3×3 box blur 羽化绿/杂边界 ----
	{
		TArray<FColor> Blurred = Pixels;
		for (int32 Y = 1; Y < Res - 1; ++Y)
		{
			for (int32 X = 1; X < Res - 1; ++X)
			{
				int32 SumR = 0, SumG = 0;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						// 高斯加权 /22：中心6，十字3，对角1 ——边界羽化更集中
						const int32 Weight = (DX == 0 && DY == 0) ? 6
										   : (DX == 0 || DY == 0) ? 3 : 1;
						const FColor& N = Pixels[(Y + DY) * Res + (X + DX)];
						SumR += N.R * Weight;
						SumG += N.G * Weight;
					}
				}
				FColor& Dst = Blurred[Y * Res + X];
				Dst.R = static_cast<uint8>(SumR / 22);
				Dst.G = static_cast<uint8>(SumG / 22);
				Dst.B = 0;
				Dst.A = 255;
			}
		}
		Pixels = MoveTemp(Blurred);
	}

	TypeTex = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
	if (!TypeTex) return;

	FTexturePlatformData* TypePlatformData = TypeTex->GetPlatformData();
	if (!TypePlatformData || TypePlatformData->Mips.Num() == 0) return;
	void* RawData = TypePlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(RawData, Pixels.GetData(), TotalPixels * sizeof(FColor));
	TypePlatformData->Mips[0].BulkData.Unlock();

	// mask 数据用线性空间 + Masks 压缩（4 通道独立）
	TypeTex->SRGB = false;
	TypeTex->CompressionSettings = TC_Masks;
#if WITH_EDITORONLY_DATA
	TypeTex->MipGenSettings = TMGS_NoMipmaps;
#endif
	TypeTex->UpdateResource();
	TypeTex->AddToRoot();
}

void UClcOpeningMaskComponent::ApplyModulationParams(UMaterialInstanceDynamic* MID)
{
	if (!MID) return;

	FRandomStream Rng(CachedSeed * 654321);

	// ---- UV 变换：随机旋转 + 偏移（同一张纹理在不同石头上朝向不同） ----
	const float RotationDeg = Rng.FRandRange(-20.0f, 20.0f);
	const float Rad = FMath::DegreesToRadians(RotationDeg);
	MID->SetScalarParameterValue(TEXT("ModUV_Cos"), FMath::Cos(Rad));
	MID->SetScalarParameterValue(TEXT("ModUV_Sin"), FMath::Sin(Rad));
	MID->SetScalarParameterValue(TEXT("ModUV_OffU"), Rng.FRandRange(-0.15f, 0.15f));
	MID->SetScalarParameterValue(TEXT("ModUV_OffV"), Rng.FRandRange(-0.15f, 0.15f));

	// ---- 种水调制（Grade → roughness 偏移） ----
	float GradeRoughBias = 0.0f;
	switch (CachedGrade)
	{
	case EClcJadeGrade::Bean:      GradeRoughBias =  0.15f; break;
	case EClcJadeGrade::Glutinous: GradeRoughBias =  0.05f; break;
	case EClcJadeGrade::Ice:       GradeRoughBias = -0.10f; break;
	case EClcJadeGrade::Glass:     GradeRoughBias = -0.20f; break;
	}
	MID->SetScalarParameterValue(TEXT("ModGradeRoughBias"), GradeRoughBias);

	UE_LOG(LogClaudeCore, Verbose, TEXT("[ClcMask] Modulation: Rot=%.1f° GradeBias=%.2f"),
		RotationDeg, GradeRoughBias);
}

// ============================================================
// 材质对接
// ============================================================

void UClcOpeningMaskComponent::ApplyToMaterial(UMaterialInstanceDynamic* DynMaterial)
{
	if (!DynMaterial) return;

	if (MaskRT)
	{
		DynMaterial->SetTextureParameterValue(TEXT("MaskRT"), MaskRT);
	}
	if (RevealTex)
	{
		DynMaterial->SetTextureParameterValue(TEXT("RevealTex"), RevealTex);
	}
	if (TypeTex)
	{
		DynMaterial->SetTextureParameterValue(TEXT("TypeTex"), TypeTex);
	}

	// ---- 调制参数（每块石头独一无二，Seed 驱动） ----
	ApplyModulationParams(DynMaterial);
}

// ============================================================
// 打磨
// ============================================================

FClcStoneOpeningResult UClcOpeningMaskComponent::GrindAtUV(float UV_U, float UV_V)
{
	FClcStoneOpeningResult Result;

	// UV → 像素坐标
	const int32 CX = FMath::Clamp(FMath::RoundToInt(UV_U * (MaskResolution - 1)), 0, MaskResolution - 1);
	const int32 CY = FMath::Clamp(FMath::RoundToInt(UV_V * (MaskResolution - 1)), 0, MaskResolution - 1);

	// 笔刷半径（像素）
	const float RadiusPixels = BrushRadius * MaskResolution;
	const int32 RadiusInt = FMath::CeilToInt(RadiusPixels);

	// 统计笔刷范围内新暴露的玉/杂质/裂像素
	int32 NewGreenPixels = 0;
	int32 NewImpurityPixels = 0;
	int32 NewCrackPixels = 0;

	// 笔刷包围盒
	const int32 X0 = FMath::Max(0, CX - RadiusInt);
	const int32 X1 = FMath::Min(MaskResolution - 1, CX + RadiusInt);
	const int32 Y0 = FMath::Max(0, CY - RadiusInt);
	const int32 Y1 = FMath::Min(MaskResolution - 1, CY + RadiusInt);

	for (int32 Y = Y0; Y <= Y1; ++Y)
	{
		for (int32 X = X0; X <= X1; ++X)
		{
			const float DX = static_cast<float>(X - CX);
			const float DY = static_cast<float>(Y - CY);
			const float Dist = FMath::Sqrt(DX * DX + DY * DY);

			if (Dist > RadiusPixels) continue;

			// 计算画笔强度（带软边）
			float Strength;
			if (BrushHardness >= 0.99f)
			{
				Strength = 1.0f;
			}
			else
			{
				const float T = FMath::Clamp(Dist / RadiusPixels, 0.0f, 1.0f);
				// BrushHardness=1→硬边（全强度到边缘）；=0→最柔和（中心到边缘全程衰减）。
				// 全强度内圈 = [0, BrushHardness]；过渡区 = [BrushHardness, 1]，宽度 1-BrushHardness。
				const float SoftStart = BrushHardness;
				const float FadeSpan = 1.0f - BrushHardness;
				if (FadeSpan <= KINDA_SMALL_NUMBER || T <= SoftStart)
				{
					Strength = 1.0f;
				}
				else
				{
					Strength = 1.0f - FMath::Square((T - SoftStart) / FadeSpan);
				}
			}

			const int32 Idx = Y * MaskResolution + X;
			const uint8 OldVal = MaskBuffer[Idx];
			const int32 NewVal = FMath::Min(255, OldVal + FMath::RoundToInt(Strength * 255.0f));

			if (NewVal > OldVal && OldVal < 128 && NewVal >= 128)
			{
				// 跨过阈值（128=半透明），露出底层
				++OpenedPixelCount;
				const uint8 MatType = CachedDistribution.GetPixel(X, Y);
				if (MatType == JadeBody)        { ++NewGreenPixels;    ++OpenedGreenPixelCount; }
				else if (MatType == Impurity)   { ++NewImpurityPixels; ++OpenedImpurityPixelCount; }
				else if (MatType == Crack)      { ++NewCrackPixels;    ++OpenedCrackPixelCount; }
			}

			MaskBuffer[Idx] = static_cast<uint8>(NewVal);
		}
	}

	// 上传到 GPU
	UploadMaskToGPU();

	const float PixelToFraction = 1.0f / static_cast<float>(MaskResolution * MaskResolution);
	const int32 NewBlackPixels = NewImpurityPixels + NewCrackPixels;
	Result.AreaFraction = FMath::Square(BrushRadius) * PI;
	Result.bHitGreen = NewGreenPixels > 0;
	Result.bHitImpurity = NewImpurityPixels > 0;
	Result.bHitCrack = NewCrackPixels > 0;
	Result.bHitBlack = NewBlackPixels > 0;
	Result.NewGreenFraction = NewGreenPixels * PixelToFraction;
	Result.NewImpurityFraction = NewImpurityPixels * PixelToFraction;
	Result.NewCrackFraction = NewCrackPixels * PixelToFraction;
	Result.NewBlackFraction = NewBlackPixels * PixelToFraction;

	return Result;
}

// ============================================================
// GPU 上传
// ============================================================

void UClcOpeningMaskComponent::UploadMaskToGPU()
{
	if (!MaskRT) return;

	// 必须在渲染线程内访问 RT 资源，把 RT 指针捕获进 lambda
	UTextureRenderTarget2D* RT = MaskRT;

	ENQUEUE_RENDER_COMMAND(UploadOpeningMask)(
		[RT, BufferData = MaskBuffer, Res = MaskResolution](FRHICommandListImmediate& RHICmdList)
		{
			if (!IsValid(RT)) return;
			FTextureRenderTarget2DResource* RTResource =
				static_cast<FTextureRenderTarget2DResource*>(RT->GetRenderTargetResource());
			if (!RTResource) return;

			FRHITexture* TextureRHI = RTResource->GetTextureRHI();
			if (!TextureRHI) return;

			const uint32 DestStride = Res;
			FUpdateTextureRegion2D Region(0, 0, 0, 0, Res, Res);
			RHIUpdateTexture2D(TextureRHI, 0, Region, DestStride, BufferData.GetData());
		});
}

// ============================================================
// 查询
// ============================================================

float UClcOpeningMaskComponent::GetOpenedRatio() const
{
	const int32 TotalPixels = MaskResolution * MaskResolution;
	if (TotalPixels == 0) return 0.0f;
	return static_cast<float>(OpenedPixelCount) / static_cast<float>(TotalPixels);
}

float UClcOpeningMaskComponent::GetExposedGreenRatio() const
{
	const int32 TotalPixels = MaskResolution * MaskResolution;
	if (TotalPixels == 0) return 0.0f;
	return static_cast<float>(OpenedGreenPixelCount) / static_cast<float>(TotalPixels);
}

float UClcOpeningMaskComponent::GetExposedImpurityRatio() const
{
	const int32 TotalPixels = MaskResolution * MaskResolution;
	if (TotalPixels == 0) return 0.0f;
	return static_cast<float>(OpenedImpurityPixelCount) / static_cast<float>(TotalPixels);
}

float UClcOpeningMaskComponent::GetExposedCrackRatio() const
{
	const int32 TotalPixels = MaskResolution * MaskResolution;
	if (TotalPixels == 0) return 0.0f;
	return static_cast<float>(OpenedCrackPixelCount) / static_cast<float>(TotalPixels);
}

// ============================================================
// 连通域分析——4 连通 BFS 找最大绿色连通域
// 每 0.3s 由 GetStoneData 调一次（Workbench HUD 刷新节奏），O(N) 微秒级
// ============================================================

int32 UClcOpeningMaskComponent::ComputeLargestGreenConnectedComponent() const
{
	const int32 Res = MaskResolution;
	const int32 Total = Res * Res;
	if (Total == 0 || MaskBuffer.Num() < Total) return 0;

	TArray<bool> Visited;
	Visited.Init(false, Total);

	int32 LargestSize = 0;

	for (int32 StartIdx = 0; StartIdx < Total; ++StartIdx)
	{
		if (Visited[StartIdx]) continue;

		const int32 SX = StartIdx % Res;
		const int32 SY = StartIdx / Res;

		// 必须是已擦石(>=128)且玉肉(CachedDistribution==JadeBody)
		if (MaskBuffer[StartIdx] < 128 || CachedDistribution.GetPixel(SX, SY) != JadeBody)
		{
			Visited[StartIdx] = true;
			continue;
		}

		// BFS——用头索引避免 TArray 头部删除的 O(N) 开销
		int32 ComponentSize = 0;
		TArray<int32> Queue;
		Queue.Reserve(256);
		Queue.Add(StartIdx);
		Visited[StartIdx] = true;
		int32 Head = 0;

		while (Head < Queue.Num())
		{
			const int32 CurIdx = Queue[Head++];
			++ComponentSize;

			const int32 CX = CurIdx % Res;
			const int32 CY = CurIdx / Res;

			// 4 邻居（边界安全）
			const int32 Left  = (CX > 0)       ? CurIdx - 1   : INDEX_NONE;
			const int32 Right = (CX < Res - 1) ? CurIdx + 1   : INDEX_NONE;
			const int32 Up    = (CY > 0)       ? CurIdx - Res : INDEX_NONE;
			const int32 Down  = (CY < Res - 1) ? CurIdx + Res : INDEX_NONE;

			const int32 Neighbors[4] = { Left, Right, Up, Down };
			for (int32 n = 0; n < 4; ++n)
			{
				const int32 NIdx = Neighbors[n];
				if (NIdx == INDEX_NONE || Visited[NIdx]) continue;

				const int32 NX = NIdx % Res;
				const int32 NY = NIdx / Res;

				if (MaskBuffer[NIdx] < 128 || CachedDistribution.GetPixel(NX, NY) != JadeBody)
				{
					Visited[NIdx] = true;
					continue;
				}

				Visited[NIdx] = true;
				Queue.Add(NIdx);
			}
		}

		LargestSize = FMath::Max(LargestSize, ComponentSize);
	}

	return LargestSize;
}
