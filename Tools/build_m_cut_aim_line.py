"""
build_m_cut_aim_line.py  —  UE 5.6 Python
构建 /Game/JadeBetting/Materials/M_CutAimLine (切石瞄准线 Decal 材质)

细线 + 两端横标，Translucent decal。
R 通道(U) 控制线宽，G 通道(V) 检测端帽位置。

用法：py "F:/UELibrary/KimmelRebirth/Tools/build_m_cut_aim_line.py"
"""
import unreal

me = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def build():
    PATH = "/Game/JadeBetting/Materials/M_CutAimLine"
    mat = eal.load_asset(PATH)
    if not mat:
        mat = at.create_asset("M_CutAimLine", "/Game/JadeBetting/Materials",
                              unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("FAILED to create or load M_CutAimLine"); return

    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    me.delete_all_material_expressions(mat)

    E = lambda cls, x, y: me.create_material_expression(mat, cls, x, y)
    F = 0

    def C(frm, fp, to, tp):
        nonlocal F
        F += not me.connect_material_expressions(frm, fp, to, tp)

    # ============================================================
    #  TexCoord → split into U(R) and V(G)
    # ============================================================
    tc = E(unreal.MaterialExpressionTextureCoordinate, -800, 0)
    tc.set_editor_property("coordinate_index", 0)

    mask_u = E(unreal.MaterialExpressionComponentMask, -700, -60)
    mask_u.set_editor_property("r", True)
    mask_u.set_editor_property("g", False)
    mask_u.set_editor_property("b", False)
    mask_u.set_editor_property("a", False)
    C(tc, "", mask_u, "")

    mask_v = E(unreal.MaterialExpressionComponentMask, -700, 160)
    mask_v.set_editor_property("r", False)
    mask_v.set_editor_property("g", True)
    mask_v.set_editor_property("b", False)
    mask_v.set_editor_property("a", False)
    C(tc, "", mask_v, "")

    # ============================================================
    #  |U - 0.5|  (distance from center along width axis)
    # ============================================================
    half = E(unreal.MaterialExpressionConstant, -620, -120)
    half.set_editor_property("r", 0.5)

    sub_center = E(unreal.MaterialExpressionSubtract, -540, -60)
    C(mask_u, "", sub_center, "A")
    C(half, "", sub_center, "B")

    abs_u = E(unreal.MaterialExpressionAbs, -440, -60)
    C(sub_center, "", abs_u, "")

    # ============================================================
    #  Main line: Step(LineWidth - |U-0.5|,  0)  →  1 when inside
    # ============================================================
    lw = E(unreal.MaterialExpressionScalarParameter, -440, -140)
    lw.set_editor_property("parameter_name", "LineWidth")
    lw.set_editor_property("default_value", 0.03)
    lw.set_editor_property("group", "CutAim")

    sub_line = E(unreal.MaterialExpressionSubtract, -320, -60)
    C(lw, "", sub_line, "A")
    C(abs_u, "", sub_line, "B")

    zero = E(unreal.MaterialExpressionConstant, -320, -140)
    zero.set_editor_property("r", 0.0)

    step_line = E(unreal.MaterialExpressionStep, -200, -60)
    C(sub_line, "", step_line, "X")
    C(zero, "", step_line, "Y")

    # ============================================================
    #  Cap parameters
    # ============================================================
    cap_v = E(unreal.MaterialExpressionScalarParameter, -620, 260)
    cap_v.set_editor_property("parameter_name", "CapSize")
    cap_v.set_editor_property("default_value", 0.06)
    cap_v.set_editor_property("group", "CutAim")

    cap_u = E(unreal.MaterialExpressionScalarParameter, -620, 340)
    cap_u.set_editor_property("parameter_name", "CapLength")
    cap_u.set_editor_property("default_value", 0.10)
    cap_u.set_editor_property("group", "CutAim")

    # ============================================================
    #  Bottom cap:  V < CapSize  AND  |U-0.5| < CapLength
    # ============================================================
    # Step(V, CapSize) → 1 when V >= CapSize
    step_bot_v = E(unreal.MaterialExpressionStep, -500, 220)
    C(mask_v, "", step_bot_v, "X")
    C(cap_v, "", step_bot_v, "Y")

    # 1 - Step(V, CapSize) → 1 when V < CapSize
    one_c = E(unreal.MaterialExpressionConstant, -420, 220)
    one_c.set_editor_property("r", 1.0)
    inv_bot = E(unreal.MaterialExpressionSubtract, -340, 220)
    C(one_c, "", inv_bot, "A")
    C(step_bot_v, "", inv_bot, "B")

    # ============================================================
    #  Top cap:  V > 1-CapSize  AND  |U-0.5| < CapLength
    # ============================================================
    om_cap = E(unreal.MaterialExpressionOneMinus, -500, 320)
    C(cap_v, "", om_cap, "")

    step_top_v = E(unreal.MaterialExpressionStep, -380, 320)
    C(mask_v, "", step_top_v, "X")
    C(om_cap, "", step_top_v, "Y")

    # ============================================================
    #  Cap U mask:  Step(CapLength - |U-0.5|, 0) → 1 when inside
    # ============================================================
    sub_cap_u = E(unreal.MaterialExpressionSubtract, -440, 410)
    C(cap_u, "", sub_cap_u, "A")
    C(abs_u, "", sub_cap_u, "B")

    step_cap_u = E(unreal.MaterialExpressionStep, -320, 410)
    C(sub_cap_u, "", step_cap_u, "X")
    C(zero, "", step_cap_u, "Y")

    # ============================================================
    #  Combine:  bot_cap = bottomV AND capU,  top_cap = topV AND capU
    # ============================================================
    bot_cap = E(unreal.MaterialExpressionMultiply, -180, 200)
    C(inv_bot, "", bot_cap, "A")
    C(step_cap_u, "", bot_cap, "B")

    top_cap = E(unreal.MaterialExpressionMultiply, -180, 300)
    C(step_top_v, "", top_cap, "A")
    C(step_cap_u, "", top_cap, "B")

    caps = E(unreal.MaterialExpressionAdd, -40, 250)
    C(bot_cap, "", caps, "A")
    C(top_cap, "", caps, "B")

    # ============================================================
    #  Final mask = line OR caps  (Max = logical OR for binary)
    # ============================================================
    final_mask = E(unreal.MaterialExpressionMax, 100, 100)
    C(step_line, "", final_mask, "A")
    C(caps, "", final_mask, "B")

    # ============================================================
    #  Color & Opacity → outputs
    # ============================================================
    col = E(unreal.MaterialExpressionVectorParameter, 80, -60)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.12, 0.05, 1.0))
    col.set_editor_property("group", "CutAim")

    op = E(unreal.MaterialExpressionScalarParameter, 80, 40)
    op.set_editor_property("parameter_name", "Opacity")
    op.set_editor_property("default_value", 1.0)
    op.set_editor_property("group", "CutAim")

    mul_e = E(unreal.MaterialExpressionMultiply, 240, -60)
    C(col, "", mul_e, "A")
    C(final_mask, "", mul_e, "B")

    mul_o = E(unreal.MaterialExpressionMultiply, 240, 40)
    C(final_mask, "", mul_o, "A")
    C(op, "", mul_o, "B")

    # ============================================================
    #  Outputs
    # ============================================================
    F += not me.connect_material_property(mul_e, "", MP.MP_EMISSIVE_COLOR)
    F += not me.connect_material_property(mul_o, "", MP.MP_OPACITY)

    me.layout_material_expressions(mat)
    me.recompile_material(mat)
    eal.save_asset(PATH)
    unreal.log(f"=== M_CutAimLine SAVED  fails={F} ===")


if __name__ == "__main__":
    build()
