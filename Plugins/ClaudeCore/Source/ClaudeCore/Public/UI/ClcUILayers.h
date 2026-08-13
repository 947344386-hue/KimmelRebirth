// Copyright ClaudeCore. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * UI 视口层级集中定义 —— 所有 AddToViewport 的 ZOrder 必须引用此命名空间下的常量。
 * 使用 constexpr int32 而非 UENUM，避免 UHT 对 uint8 基类型的限制（Tooltip=1000 超出范围）。
 *
 * 层级从低到高（数值越大越在上层）。语义分组：
 *   底层全屏    MainMenu=0
 *   世界 HUD    Reticle=10（准星/交互指示）
 *   弹层 UI     Haggle=20（讨价还价 QTE）
 *   常驻面板    QuestTracker=40（任务追踪）
 *   常驻 HUD    GoldHud=50（金币 HUD）
 *   信息卡      StoneInfo=60（石头信息卡）
 *   菜单        ToolUpgrade=80（工具升级菜单）
 *   弹层面板    Backpack=100（背包）/ Toast=100（日志提示）
 *   提示        KeyPrompt=110（按键提示）
 *   对话浮层    OverlayPanel=120（传送菜单/商人气泡/金币飞行特效）
 *   商人附加    MerchantEagleEye=121（鹰眼气泡）/ MerchantArrow=122（屏外箭头）
 *   系统菜单    PauseMenu=150（暂停菜单）
 *   模态弹窗    SaveSlotList=160（存档槽位列表）
 *   顶层        Tooltip=1000（石头工具提示）
 */
struct FClcUIZOrder
{
	static constexpr int32 MainMenu = 0;
	static constexpr int32 Reticle = 10;
	static constexpr int32 Haggle = 20;
	static constexpr int32 QuestTracker = 40;
	static constexpr int32 GoldHud = 50;
	static constexpr int32 StoneInfo = 60;
	static constexpr int32 ToolUpgrade = 80;
	static constexpr int32 Backpack = 100;
	static constexpr int32 Toast = 100;
	static constexpr int32 KeyPrompt = 110;
	static constexpr int32 OverlayPanel = 120;
	static constexpr int32 MerchantEagleEye = 121;
	static constexpr int32 MerchantArrow = 122;
	static constexpr int32 PauseMenu = 150;
	static constexpr int32 SaveSlotList = 160;
	static constexpr int32 Tooltip = 1000;
};