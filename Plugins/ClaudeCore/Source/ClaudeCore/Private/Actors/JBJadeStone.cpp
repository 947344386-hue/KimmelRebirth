#include "Actors/JBJadeStone.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"

AJBJadeStone::AJBJadeStone()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	CrustSM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crust"));
	CoreSM  = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	CrustSM->SetupAttachment(Root);
	CoreSM->SetupAttachment(Root);
	CrustSM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CrustSM->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CoreSM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AJBJadeStone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshAndScales();
	EnsureRuntimeResources();
	ApplyMaterials();
}

void AJBJadeStone::BeginPlay()
{
	Super::BeginPlay();
	ApplyMeshAndScales();
	EnsureRuntimeResources();
	ApplyMaterials();
}

#if WITH_EDITOR
void AJBJadeStone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName Prop = PropertyChangedEvent.GetPropertyName();
	if (Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, BaseMesh) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, CrustScale) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, CoreScale))
		ApplyMeshAndScales();
	if (Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, CrustMaterial) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, CoreMaterial))
		ApplyMaterials();
	if (Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, MaskResolution) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, NoiseTextureSize) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, NoiseScale) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, NoiseOctaves) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, NoiseSeed) ||
		Prop == GET_MEMBER_NAME_CHECKED(FJBJadeStoneConfig, NoiseContrast))
	{
		EnsureRuntimeResources();
		ApplyMaterials();
	}
}
#endif

void AJBJadeStone::ApplyMeshAndScales()
{
	if (!StoneConfig.BaseMesh) return;
	CrustSM->SetStaticMesh(StoneConfig.BaseMesh);
	CoreSM->SetStaticMesh(StoneConfig.BaseMesh);
	CrustSM->SetWorldScale3D(FVector(StoneConfig.CrustScale));
	CoreSM->SetWorldScale3D(FVector(StoneConfig.CoreScale));
}

void AJBJadeStone::EnsureRuntimeResources()
{
	const int32 Res = FMath::Max(16, StoneConfig.MaskResolution);
	bool bRecreateRT = !CrustMaskRT || CrustMaskRT->SizeX != Res;
	if (bRecreateRT)
	{
		CrustMaskRT = NewObject<UTextureRenderTarget2D>(this);
		CrustMaskRT->InitCustomFormat(Res, Res, PF_B8G8R8A8, false);
		CrustMaskRT->UpdateResource();
	}
	if (bRecreateRT || MaskAccumulator.Num() != Res * Res)
	{
		const int32 N = Res * Res;
		MaskAccumulator.SetNum(N);
		for (int32 i = 0; i < N; ++i) MaskAccumulator[i] = FColor::White;
		bMaskDirty = true;
	}
	if (bMaskDirty) { UploadMaskToRT(); FlushRenderingCommands(); bMaskDirty = false; }

	const int32 Size = StoneConfig.NoiseTextureSize;
	if (!NoiseTexture || NoiseTexture->GetSizeX() != Size || NoiseTexture->GetSizeY() != Size)
		NoiseTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);

	if (NoiseTexture && NoiseTexture->GetPlatformData())
	{
		FTexturePlatformData* PD = NoiseTexture->GetPlatformData();
		PD->SizeX = Size; PD->SizeY = Size; PD->PixelFormat = PF_B8G8R8A8;
		if (PD->Mips.Num() == 0) PD->Mips.Emplace();
		FTexture2DMipMap& Mip = PD->Mips[0];
		uint8* Data = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		const int32 Oct = StoneConfig.NoiseOctaves, Seed = StoneConfig.NoiseSeed;
		const float Scl = StoneConfig.NoiseScale, Con = StoneConfig.NoiseContrast;
		for (int32 y = 0; y < Size; ++y)
			for (int32 x = 0; x < Size; ++x)
			{
				float N = PerlinNoise2D((float)x/Size*Scl, (float)y/Size*Scl, Seed, Oct, 0.5f);
				float Raw = FMath::Clamp(N*0.5f+0.5f, 0.0f, 1.0f);
				float Pushed = FMath::Clamp((Raw-0.5f)*Con+0.5f, 0.0f, 1.0f);
				uint8 V = (uint8)(Pushed*255.0f);
				int32 Idx = (y*Size+x)*4;
				Data[Idx]=V; Data[Idx+1]=V; Data[Idx+2]=V; Data[Idx+3]=255;
			}
		Mip.BulkData.Unlock();
		NoiseTexture->AddressX = TA_Wrap; NoiseTexture->AddressY = TA_Wrap;
		NoiseTexture->SRGB = false; NoiseTexture->UpdateResource();
	}
}

void AJBJadeStone::ApplyMaterials()
{
	if (StoneConfig.CrustMaterial && CrustSM)
	{
		CrustMI = UMaterialInstanceDynamic::Create(StoneConfig.CrustMaterial, this);
		if (CrustMaskRT) CrustMI->SetTextureParameterValue(FName("CrustMask"), CrustMaskRT);
		CrustSM->SetMaterial(0, CrustMI);
	}
	if (StoneConfig.CoreMaterial && CoreSM)
	{
		CoreMI = UMaterialInstanceDynamic::Create(StoneConfig.CoreMaterial, this);
		if (NoiseTexture) CoreMI->SetTextureParameterValue(FName("NoiseTex"), NoiseTexture);
		CoreSM->SetMaterial(0, CoreMI);
	}
}

void AJBJadeStone::RotateStone(float Yaw, float Pitch)
{
	FRotator Delta = FRotator(Pitch, Yaw, 0.0f) * StoneConfig.RotationSpeed * GetWorld()->GetDeltaSeconds();
	AddActorLocalRotation(Delta.Quaternion());
}

float AJBJadeStone::GetOpenedRatio() const
{
	if (MaskAccumulator.Num() == 0) return 0.0f;
	if (bMaskDirty)
	{
		int32 Opened = 0;
		for (const FColor& P : MaskAccumulator) if (P.R < 128) ++Opened;
		const_cast<AJBJadeStone*>(this)->CachedOpenedRatio = (float)Opened / MaskAccumulator.Num();
		const_cast<AJBJadeStone*>(this)->bMaskDirty = false;
	}
	return CachedOpenedRatio;
}

void AJBJadeStone::PaintMaskAtUV(FVector2D UV, float BrushRadius)
{
	if (MaskAccumulator.Num() == 0) return;
	const int32 Size = FMath::RoundToInt(FMath::Sqrt((float)MaskAccumulator.Num()));
	const int32 CX = FMath::Clamp((int32)(UV.X*Size), 0, Size-1);
	const int32 CY = FMath::Clamp((int32)(UV.Y*Size), 0, Size-1);
	const int32 R = FMath::Max(1, (int32)(BrushRadius*Size));
	for (int32 dy = -R; dy <= R; ++dy)
		for (int32 dx = -R; dx <= R; ++dx)
		{
			float Dist = FMath::Sqrt((float)(dx*dx+dy*dy));
			if (Dist > (float)R) continue;
			int32 PX = CX+dx, PY = CY+dy;
			if (PX<0||PX>=Size||PY<0||PY>=Size) continue;
			float T = FMath::SmoothStep((float)R*0.25f, (float)R, Dist);
			uint8 Erase = (uint8)FMath::Clamp(T*255.0f, 0.0f, 255.0f);
			FColor& P = MaskAccumulator[PY*Size+PX];
			P.R=FMath::Min(P.R,Erase);P.G=FMath::Min(P.G,Erase);
			P.B=FMath::Min(P.B,Erase);P.A=FMath::Min(P.A,Erase);
		}
	bMaskDirty = true;
	UploadMaskToRT();
}

void AJBJadeStone::UploadMaskToRT()
{
	FTextureRenderTargetResource* RTRes = CrustMaskRT ? CrustMaskRT->GameThread_GetRenderTargetResource() : nullptr;
	if (!RTRes) return;
	TArray<FColor> Copy = MaskAccumulator;
	ENQUEUE_RENDER_COMMAND(JBUploadMask)([RTRes, Copy = MoveTemp(Copy)](FRHICommandListImmediate& RHICmdList)
	{
		FRHITexture2D* Tex = RTRes->GetRenderTargetTexture().GetReference();
		if (!Tex) return;
		uint32 Stride = 0;
		void* Dest = RHICmdList.LockTexture2D(Tex, 0, RLM_WriteOnly, Stride, false);
		if (Dest) { FMemory::Memcpy(Dest, Copy.GetData(), Copy.Num()*sizeof(FColor)); RHICmdList.UnlockTexture2D(Tex, 0, false); }
	});
}

float AJBJadeStone::Hash2D(int32 X, int32 Y, int32 S) { int32 H=S*374761393+X*668265263+Y*1440664723; H=(H^(H>>13))*1274126177; return (float)((uint32)(H^(H>>16))&0x7fffffff)/(float)0x7fffffff; }
float AJBJadeStone::SmoothNoise2D(int32 X, int32 Y, int32 S) { return (Hash2D(X-1,Y-1,S)+Hash2D(X+1,Y-1,S)+Hash2D(X-1,Y+1,S)+Hash2D(X+1,Y+1,S))/16.f+(Hash2D(X-1,Y,S)+Hash2D(X+1,Y,S)+Hash2D(X,Y-1,S)+Hash2D(X,Y+1,S))/8.f+Hash2D(X,Y,S)/4.f; }
float AJBJadeStone::InterpolatedNoise2D(float X, float Y, int32 S) { int32 IX=FMath::FloorToInt(X),IY=FMath::FloorToInt(Y); float FX=X-IX,FY=Y-IY,SX=FX*FX*(3-2*FX),SY=FY*FY*(3-2*FY); return FMath::Lerp(FMath::Lerp(SmoothNoise2D(IX,IY,S),SmoothNoise2D(IX+1,IY,S),SX),FMath::Lerp(SmoothNoise2D(IX,IY+1,S),SmoothNoise2D(IX+1,IY+1,S),SX),SY); }
float AJBJadeStone::PerlinNoise2D(float X, float Y, int32 S, int32 Oct, float P) { float T=0,F=1,A=1,M=0; for(int32 i=0;i<Oct;++i){T+=InterpolatedNoise2D(X*F,Y*F,S+i*997)*A;M+=A;F*=2;A*=P;} return M>0?T/M:0; }