"""
build_m_stone_cut_face.py  —  UE 5.6 Python, 最简版 v1
构建 /Game/JadeBetting/Materials/M_StoneCutFace

核心逻辑：
  VertexColor.G → Step(JadeThreshold) → JadeMask
  JadeMask → 3×Lerp(Junk, Jade) → BaseColor / Normal / ORM
  ORM → 拆 R/G/B → AO / Roughness / Metallic

不含 Noise 扰动——切面三角扇放射问题你手动加。
"""
import unreal

me  = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
at  = unreal.AssetToolsHelpers.get_asset_tools()
MP  = unreal.MaterialProperty


def _node(mat, cls, x=0, y=0):
    return me.create_material_expression(mat, cls, x, y)


def _scalar(mat, name, default, group="", x=0, y=0):
    n = _node(mat, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("default_value", default)
    n.set_editor_property("group", group)
    return n


def _tex(mat, name, default_path, group="", x=0, y=0):
    n = _node(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property("parameter_name", name)
    n.set_editor_property("group", group)
    if default_path:
        tex = eal.load_asset(default_path)
        if tex:
            n.set_editor_property("texture", tex)
    return n


def _mask(mat, r=False, g=False, b=False, a=False, x=0, y=0):
    n = _node(mat, unreal.MaterialExpressionComponentMask, x, y)
    n.set_editor_property("r", r); n.set_editor_property("g", g)
    n.set_editor_property("b", b); n.set_editor_property("a", a)
    return n


def build():
    PATH   = "/Game/JadeBetting/Materials/M_StoneCutFace"
    G      = "CutFace"
    DEF_BC = "/Engine/EngineMaterials/DefaultWhiteGrid"
    DEF_N  = "/Engine/EngineMaterials/DefaultNormal"

    mat = eal.load_asset(PATH)
    if not mat:
        mat = at.create_asset("M_StoneCutFace", "/Game/JadeBetting/Materials",
                              unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_error("FAILED to create material"); return

    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    me.delete_all_material_expressions(mat)

    F = 0  # failure counter

    # ========================================================================
    #  Col -7: 源节点
    # ========================================================================
    tc0 = _node(mat, unreal.MaterialExpressionTextureCoordinate, -700, -120)
    tc0.set_editor_property("coordinate_index", 0)

    vc = _node(mat, unreal.MaterialExpressionVertexColor, -700, 160)

    # ========================================================================
    #  Col -6: Scalar 参数
    # ========================================================================
    jade_thr = _scalar(mat, "JadeThreshold", 0.5, G, -600, -40)

    # ========================================================================
    #  Col -5: VertexColor → Step binarization
    # ========================================================================
    vcg = _mask(mat, r=False, g=True, b=False, a=False, x=-500, y=-40)

    step_j = _node(mat, unreal.MaterialExpressionStep, -400, -40)
    # Step(X=threshold, Y=VC.G): output=1 when VC.G >= threshold, else 0

    # ========================================================================
    #  Col -4: 6 TextureObject 参数 (MID 注入目标)
    # ========================================================================
    j_bc  = _tex(mat, "JadeBaseColor", DEF_BC, G, -300, -300)
    j_n   = _tex(mat, "JadeNormal",    DEF_N,  G, -300, -200)
    j_orm = _tex(mat, "JadeORM",       DEF_BC, G, -300, -100)
    k_bc  = _tex(mat, "JunkBaseColor", DEF_BC, G, -300,   80)
    k_n   = _tex(mat, "JunkNormal",    DEF_N,  G, -300,  180)
    k_orm = _tex(mat, "JunkORM",       DEF_BC, G, -300,  280)

    # ========================================================================
    #  Col -2: 3× Lerp (A=Junk, B=Jade, Alpha=JadeMask)
    # ========================================================================
    lbc  = _node(mat, unreal.MaterialExpressionLinearInterpolate, -150, -260)
    ln   = _node(mat, unreal.MaterialExpressionLinearInterpolate, -150, -160)
    lorm = _node(mat, unreal.MaterialExpressionLinearInterpolate, -150,  -60)

    # ========================================================================
    #  Col 0: ORM → 拆 R/G/B
    # ========================================================================
    o_r = _mask(mat, r=True,  g=False, b=False, a=False, x=50, y=-60)
    o_g = _mask(mat, r=False, g=True,  b=False, a=False, x=50, y= 20)
    o_b = _mask(mat, r=False, g=False, b=True,  a=False, x=50, y=100)

    # ========================================================================
    #                         CONNECTIONS
    # ========================================================================
    def C(frm, frm_pin, to, to_pin):
        nonlocal F
        ok = me.connect_material_expressions(frm, frm_pin, to, to_pin)
        if not ok:
            ins = me.get_material_expression_input_names(to)
            unreal.log_warning(
                f"  CONN FAIL: {frm.__class__.__name__}.{frm_pin} → {to.__class__.__name__}.{to_pin}"
                f"  | avail inputs={ins}")
            F += 1

    # VertexColor → Mask.G
    C(vc, "", vcg, "")

    # Step: X=JadeThreshold, Y=VertexColor.G
    C(jade_thr, "", step_j, "X")
    C(vcg,      "", step_j, "Y")

    # 6 贴图 ← TexCoord UV
    for t in (j_bc, j_n, j_orm, k_bc, k_n, k_orm):
        C(tc0, "", t, "UVs")

    # 3× Lerp: A=Junk, B=Jade, Alpha=JadeMask(Step输出)
    C(k_bc,  "", lbc,  "A")
    C(j_bc,  "", lbc,  "B")
    C(step_j,"", lbc,  "Alpha")

    C(k_n,   "", ln,   "A")
    C(j_n,   "", ln,   "B")
    C(step_j, "", ln,   "Alpha")

    C(k_orm, "", lorm, "A")
    C(j_orm, "", lorm, "B")
    C(step_j, "", lorm, "Alpha")

    # ORM → 拆通道
    C(lorm, "", o_r, "")
    C(lorm, "", o_g, "")
    C(lorm, "", o_b, "")

    # ========================================================================
    #                     MATERIAL OUTPUTS
    # ========================================================================
    if not me.connect_material_property(lbc, "", MP.MP_BASE_COLOR):        F += 1
    if not me.connect_material_property(ln,  "", MP.MP_NORMAL):            F += 1
    if not me.connect_material_property(o_r, "", MP.MP_AMBIENT_OCCLUSION): F += 1
    if not me.connect_material_property(o_g, "", MP.MP_ROUGHNESS):         F += 1
    if not me.connect_material_property(o_b, "", MP.MP_METALLIC):          F += 1

    # ---- compile & save ----
    me.layout_material_expressions(mat)
    me.recompile_material(mat)
    eal.save_asset(PATH)
    unreal.log(f"=== M_StoneCutFace SAVED  conn_fails={F} ===")


if __name__ == "__main__":
    build()
