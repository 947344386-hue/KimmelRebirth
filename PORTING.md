# KimmelRebirth — 赌石(JadeBetting)玩法移植指引

本仓库是完整 UE 5.6 工程，含赌石玩法（ClaudeCore 插件 + JadeBetting 内容）+ 宿主（ThirdPerson 模板/变体）。两种用法。

## 方式 A：直接用本工程
1. `git clone` 本仓。
2. 用 **UE 5.6** 打开 `KimmelRebirth.uproject`（首次打开弹"项目需编译，是否编译？"→**是**）。
3. 确认 ClaudeCore 插件已启用（Settings → Plugins）。
4. 打开 `Content/JadeBetting/Level/Map_JadePlayTest.umap` → Play In Editor。
5. 验证：摊位生成石头、屏幕中心准星近=空圈/选中=圈+十字+Prompt、购买进背包、商人嘴上气泡有种水叫卖、Q 鹰眼、F 进工作台开窗、回收商 F 出售。

## 方式 B：只把赌石并入你的 UE 项目
1. 复制 `Plugins/ClaudeCore/` → 你项目的 `Plugins/ClaudeCore/`。
2. 复制 `Content/JadeBetting/` → 你项目的 `Content/JadeBetting/`。
3. 启用 ClaudeCore 插件（Settings → Plugins → 重启编辑器）。
4. 编译项目。
5. **角色接线**：给玩家角色 BP 加 `ClcInteractionComponent`（屏幕中心准星自动加载 `/Game/JadeBetting/UI/WBP_Reticle`；`ReticleWidgetClass` 留空即用默认）。准星贴图 `T_ReticleRing`（外圈）+ `T_ReticleCross`（十字），同 DrawSize 叠放。
6. **摆 Actor**：`BP_StoneStall`（生成石头+商人）、`BP_StoneVendor`（出售）、`BP_JadeWorkbench`（开窗）、`BP_ToolRepairStation`（修工具耐久）。

## 输入
赌石交互走各 Actor 的 `FKey` 字段**直接轮询**（BP Details 可改键，不依赖 EnhancedInput IA）：
- 工作台 `AClcJadeWorkbench`：F=进入开窗 / Esc=退出 / B=背包选石 / T=切工具 / **滚轮=笔刷大小** / 右键=FOV 放大
- 回收商 `AClcStoneVendor`：F=进入出售 / Esc=退出
- 修理站 `AClcToolRepairStation`：F=支付金币修复配置的工具耐久（走近 + 摄像机瞄准触发）
- 石头购买：BP 绑键调 `AClcStone::PurchaseStone`
- 鹰眼：Q 调 `UClcEagleEyeComponent::ActivateEagleEye`
- 角色移动/视角：用你项目自带输入

## DA 资产路径
集中在 `UClcDeveloperSettings`（Project Settings → Plugins → ClaudeCore），默认 `/Game/JadeBetting/Data/`。JadeBetting 换位置只改这里，不动代码。

## 关键 C++ 入口（ClaudeCore 插件）
- 市场/定价：`UClcStoneMarketSubsystem`（GameInstanceSubsystem）
- 石头/摊位：`AClcStone`、`AClcStoneStall`
- 背包：`UClcBackpackSubsystem`（LocalPlayerSubsystem，上限 200 格）
- 工作台/开窗：`AClcJadeWorkbench`、`UClcOpeningMaskComponent`、`AClcOpeningTool`、`AClcFlashlightTool`
- 工具耐久持久化：`UClcToolDurabilitySubsystem`（LocalPlayerSubsystem，跨工作台会话保存每种工具的耐久；max 由工具实例注册，BP 可配）
- 工具修理站：`AClcToolRepairStation`（可放置 Actor，Bitmask 多选修复工具类型 + 金币消耗）
- 商人/鹰眼：`AClcMerchant`、`UClcEagleEyeComponent`
- 回收：`AClcStoneVendor`
- 中心准星/交互收敛：`UClcInteractionComponent`、`UClcInteractionIndicator`、`IClcInteractable`

## 注意
- 工作台/回收商准星"选中态"需**背包有石头**（`QueryCanSelect` 门控；空背包只显空圈，设计如此）。
- 修理站准星"选中态"需**有工具待修复**且摄像机瞄准命中（aim 模式，`StationMesh` 需 QueryOnly 响应 Visibility 让球扫命中）。
- 工具耐久跨工作台会话持久化：进工作台 Spawn 工具时从 `UClcToolDurabilitySubsystem` 读取，消耗时写回；修理站按 Bitmask 恢复。工具的 `MaxDurability` 在 BP 配，Subsystem 不硬编码 max。
- 商人瞄准反应读 `UClcInteractionComponent::GetLookedAtActor`——角色须挂 `ClcInteractionComponent`，否则回退旧自检（仍可用）。
- 价格逻辑统一在 `UClcStoneMarketSubsystem`，勿在 Stone/Vendor/商人/UI 复制定价公式。
