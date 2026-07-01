// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ClcJadeTextureConfig.generated.h"

/**
 * 单一材质（玉或杂）的高保真 PBR 纹理集。
 * 约定：BaseColor(sRGB) + Normal + ORM 打包(R=AO, G=Roughness, B=Metallic)。
 * 玉和杂共用此结构，配置时各填一套。
 */
USTRUCT(BlueprintType)
struct FClcJadeTextureSet
{
    GENERATED_BODY()

    /** 基础色贴图（sRGB） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texture")
    TSoftObjectPtr<UTexture2D> BaseColor;

    /** 法线贴图 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texture")
    TSoftObjectPtr<UTexture2D> Normal;

    /** ORM 打包贴图（R=AO, G=Roughness, B=Metallic） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Texture")
    TSoftObjectPtr<UTexture2D> ORM;
};

/**
 * 玉石/杂质的高保真纹理配置 DataAsset。
 *
 * 用途：开窗材质 M_StoneOpening 不再用程序化 RevealTex 涂色，
 * 改为按 TypeTex（R=玉mask, G=杂mask）在 Jade/Junk 两套 PBR 纹理间 lerp。
 * 程序化层降级为调制（色调偏移/UV旋转/边界羽化），见阶段3。
 *
 * 资产路径约定：/Game/JadeBetting/Data/DA_JadeTextureConfig
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcJadeTextureConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 绿玉纹理集 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jade")
    FClcJadeTextureSet JadeSet;

    /** 杂裂纹理集 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jade")
    FClcJadeTextureSet JunkSet;

    /**
     * 把玉/杂两套纹理注入开窗 MID。
     * 约定参数名：JadeBaseColor / JadeNormal / JadeORM
     *            JunkBaseColor / JunkNormal / JunkORM
     * 返回成功注入的贴图数量（0~6）。
     */
    int32 InjectIntoMID(UMaterialInstanceDynamic* MID) const
    {
        if (!MID) return 0;
        int32 Transferred = 0;

        // ---- 玉套 ----
        if (UTexture2D* Tex = JadeSet.BaseColor.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JadeBaseColor"), Tex);
            ++Transferred;
        }
        if (UTexture2D* Tex = JadeSet.Normal.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JadeNormal"), Tex);
            ++Transferred;
        }
        if (UTexture2D* Tex = JadeSet.ORM.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JadeORM"), Tex);
            ++Transferred;
        }

        // ---- 杂套 ----
        if (UTexture2D* Tex = JunkSet.BaseColor.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JunkBaseColor"), Tex);
            ++Transferred;
        }
        if (UTexture2D* Tex = JunkSet.Normal.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JunkNormal"), Tex);
            ++Transferred;
        }
        if (UTexture2D* Tex = JunkSet.ORM.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("JunkORM"), Tex);
            ++Transferred;
        }

        UE_LOG(LogTemp, Log, TEXT("[ClcJadeTexture] Injected %d/6 textures into MID"), Transferred);
        return Transferred;
    }
};
