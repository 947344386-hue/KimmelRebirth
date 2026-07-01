// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ClcShellTextureConfig.generated.h"

/**
 * 单种皮壳的纹理集——设计师在编辑器里配贴图。
 * 约定：BaseColor(sRGB) + Normal + ORM 打包(R=AO, G=Roughness, B=Metallic)
 */
USTRUCT(BlueprintType)
struct FClcShellTextureEntry
{
    GENERATED_BODY()

    /** 皮壳名称（黄沙皮、黑乌沙等，纯展示用） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
    FName ShellName = NAME_None;

    /** 皮壳基础色贴图 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
    TSoftObjectPtr<UTexture2D> BaseColor;

    /** 皮壳法线贴图 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
    TSoftObjectPtr<UTexture2D> Normal;

    /** ORM 打包贴图（R=AO, G=Roughness, B=Metallic），不填时走 DefaultRoughness / DefaultMetallic */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
    TSoftObjectPtr<UTexture2D> ORM;

    /** 无 ORM 贴图时的默认粗糙度（ORM 存在时此值忽略） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell|Fallback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DefaultRoughness = 0.7f;

    /** 无 ORM 贴图时的默认金属度 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell|Fallback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DefaultMetallic = 0.0f;
};

/**
 * 皮壳纹理配置 DataAsset。
 * 石头生成时随机一个 ShellTypeIndex，运行时据此取贴图注入材质。
 * 资产路径约定：/Game/JadeBetting/Data/DA_ShellTextureConfig
 */
UCLASS(BlueprintType)
class CLAUDECORE_API UClcShellTextureConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 所有可选的皮壳类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shell")
    TArray<FClcShellTextureEntry> ShellEntries;

    /** 按索引取条目，越界返回 nullptr */
    const FClcShellTextureEntry* GetEntryByIndex(int32 Index) const
    {
        return ShellEntries.IsValidIndex(Index) ? &ShellEntries[Index] : nullptr;
    }

    /** 随机一个皮壳索引，空表返回 0 */
    UFUNCTION(BlueprintCallable, Category = "Shell")
    int32 GetRandomShellIndex() const
    {
        return ShellEntries.Num() > 0 ? FMath::RandRange(0, ShellEntries.Num() - 1) : 0;
    }

    /** 按索引查皮壳名称，BP 中直接调用（自动加载配置） */
    UFUNCTION(BlueprintCallable, Category = "Shell")
    static FName GetShellName(int32 ShellTypeIndex)
    {
        const UClcShellTextureConfig* Cfg = LoadObject<UClcShellTextureConfig>(
            nullptr, TEXT("/Game/JadeBetting/Data/DA_ShellTextureConfig"));
        if (Cfg)
        {
            if (const FClcShellTextureEntry* Entry = Cfg->GetEntryByIndex(ShellTypeIndex))
            {
                return Entry->ShellName;
            }
        }
        return NAME_None;
    }

    /**
     * 把指定皮壳类型的贴图注入到材质 MID。
     * 调用方负责先 LoadObject 本配置 + 创建好 MID。
     * 约定参数名：ShellBaseColor / ShellNormal / ShellORM
     * 返回成功注入的贴图数量（0~3）。
     */
    int32 InjectTexturesIntoMID(UMaterialInstanceDynamic* MID, int32 ShellTypeIndex) const
    {
        if (!MID) return 0;

        const FClcShellTextureEntry* Entry = GetEntryByIndex(ShellTypeIndex);
        if (!Entry)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ShellTexture] ShellTypeIndex %d not found!"), ShellTypeIndex);
            return 0;
        }

        int32 Transferred = 0;

        // BaseColor——必须，无贴图时也尽量设（后续可加默认色 fallback）
        if (UTexture2D* Tex = Entry->BaseColor.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("ShellBaseColor"), Tex);
            ++Transferred;
        }

        // Normal——有则传贴图，无则设强度 0（材质里 BlendAngleCorrectedNormals 乘系数后回退到顶点法线）
        if (UTexture2D* Tex = Entry->Normal.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("ShellNormal"), Tex);
            MID->SetScalarParameterValue(TEXT("ShellNormalStrength"), 1.0f);
            ++Transferred;
        }
        else
        {
            MID->SetScalarParameterValue(TEXT("ShellNormalStrength"), 0.0f);
        }

        // ORM——有则传贴图，无则用标量兜底 Roughness/Metallic
        if (UTexture2D* Tex = Entry->ORM.LoadSynchronous())
        {
            MID->SetTextureParameterValue(TEXT("ShellORM"), Tex);
            MID->SetScalarParameterValue(TEXT("ShellORMStrength"), 1.0f);
            ++Transferred;
        }
        else
        {
            MID->SetScalarParameterValue(TEXT("ShellORMStrength"), 0.0f);
        }

        // 兜底标量——无论有无 ORM 都设，材质按 ShellORMStrength lerp 选用
        MID->SetScalarParameterValue(TEXT("ShellRoughness"), Entry->DefaultRoughness);
        MID->SetScalarParameterValue(TEXT("ShellMetallic"), Entry->DefaultMetallic);

        UE_LOG(LogTemp, Log,
            TEXT("[ShellTexture] Injected %d/3 into MID (Type=%d, Name=%s, Rough=%.2f)"),
            Transferred, ShellTypeIndex, *Entry->ShellName.ToString(), Entry->DefaultRoughness);

        return Transferred;
    }
};
