// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "RawIndexBuffer.h"

// 打包版（cooked）网格若未勾选 Allow CPU Access，索引缓冲的 CPU 侧数据会被丢弃：
// GetNumIndices() 仍返回缓存元数据，但 GetIndex() 因实际存储为空而断言崩溃
// （引擎 StaticMesh.cpp: bNeedsCPUAccess = !RequiresCookedData() || bAllowCPUAccess）。
// 以实际存储字节数反推真正可读的索引数——为 0 即数据不可用，调用方应优雅降级。
inline int32 ClcGetAvailableIndexCount(const FRawStaticIndexBuffer& IndexBuffer)
{
	return IndexBuffer.GetIndexDataSize() / (IndexBuffer.Is32Bit() ? 4 : 2);
}
