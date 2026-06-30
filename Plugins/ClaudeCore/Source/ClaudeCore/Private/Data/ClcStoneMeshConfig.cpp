// Copyright ClaudeCore. All Rights Reserved.

#include "Data/ClcStoneMeshConfig.h"
#include "Engine/StaticMesh.h"

UStaticMesh* UClcStoneMeshConfig::GetRandomMesh() const
{
	if (StoneMeshes.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, StoneMeshes.Num() - 1);
	return StoneMeshes[Index].LoadSynchronous();
}
