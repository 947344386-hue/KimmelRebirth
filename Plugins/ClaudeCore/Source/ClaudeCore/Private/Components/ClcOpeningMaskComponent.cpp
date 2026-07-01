// Copyright ClaudeCore. All Rights Reserved.

#include "Components/ClcOpeningMaskComponent.h"
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
		// 从存档重算已开窗像素数
		OpenedPixelCount = 0;
		for (const uint8 Val : MaskBuffer)
		{
			if (Val >= 128) ++OpenedPixelCount;
		}
		EnsureMaskRT();
		UploadMaskToGPU();
		ensure(RevealTex || CachedDistribution.Data.Num() > 0);
		UE_LOG(LogTemp, Log, TEXT("[ClcMask] Restored mask from saved data (%d pixels)"),
			InData.SavedMaskBuffer.Num());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[ClcMask] No saved mask data, starting fresh."));
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

			if (Val == 1) // 绿玉
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

	void* RawData = RevealTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(RawData, Pixels.GetData(), TotalPixels * sizeof(FColor));
	RevealTex->GetPlatformData()->Mips[0].BulkData.Unlock();

	RevealTex->SRGB = true;
	RevealTex->CompressionSettings = TC_Default;
	RevealTex->MipGenSettings = TMGS_NoMipmaps;
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

			if (Val == 1)       // 绿玉
			{
				Pixel = FColor(255, 0, 0, 255);   // R=255
			}
			else if (Val == 2)  // 杂裂
			{
				Pixel = FColor(0, 255, 0, 255);   // G=255
			}
			else                // 皮壳(0)，理论上 Generate 后不存在，保险填黑
			{
				Pixel = FColor(0, 0, 0, 255);
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

	void* RawData = TypeTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(RawData, Pixels.GetData(), TotalPixels * sizeof(FColor));
	TypeTex->GetPlatformData()->Mips[0].BulkData.Unlock();

	// mask 数据用线性空间 + Masks 压缩（4 通道独立）
	TypeTex->SRGB = false;
	TypeTex->CompressionSettings = TC_Masks;
	TypeTex->MipGenSettings = TMGS_NoMipmaps;
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

	UE_LOG(LogTemp, Verbose, TEXT("[ClcMask] Modulation: Rot=%.1f° GradeBias=%.2f"),
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

	// 统计笔刷范围内新暴露的绿/黑像素
	int32 NewGreenPixels = 0;
	int32 NewBlackPixels = 0;

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
				// Hardness 越大，过渡越陡
				const float SoftStart = 1.0f - BrushHardness;
				if (T <= SoftStart)
				{
					Strength = 1.0f;
				}
				else
				{
					Strength = 1.0f - FMath::Square((T - SoftStart) / BrushHardness);
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
				if (MatType == 1) ++NewGreenPixels;
				else if (MatType == 2) ++NewBlackPixels;
			}

			MaskBuffer[Idx] = static_cast<uint8>(NewVal);
		}
	}

	// 上传到 GPU
	UploadMaskToGPU();

	const float PixelToFraction = 1.0f / static_cast<float>(MaskResolution * MaskResolution);
	Result.AreaFraction = FMath::Square(BrushRadius) * PI * 0.5f;
	Result.bHitGreen = NewGreenPixels > 0;
	Result.bHitBlack = NewBlackPixels > 0;
	Result.NewGreenFraction = NewGreenPixels * PixelToFraction;
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
