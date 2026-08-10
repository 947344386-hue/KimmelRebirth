"""
diagnose_pins.py — 打印 M_StoneCutFace 中每个节点及其输入 pin 名和连接状态。
UE5 Editor 中执行：py "F:/UELibrary/KimmelRebirth/Tools/diagnose_pins.py"
"""
import unreal

me = unreal.MaterialEditingLibrary

asset_path = "/Game/JadeBetting/Materials/M_StoneCutFace"
mat = unreal.EditorAssetLibrary.load_asset(asset_path)
if not mat:
    print("Material not found")
    raise SystemExit

# 收集所有表达式
exprs = mat.get_expressions()
print(f"Total nodes: {len(exprs)}")
print("=" * 80)

for expr in exprs:
    cls_name = expr.__class__.__name__
    inputs = me.get_material_expression_input_names(expr)
    outputs = me.get_material_expression_output_names(expr)
    print(f"\n[{cls_name}]  (outputs: {outputs})")

    for pin in inputs:
        # 检查连接
        connected = me.get_material_expression_input_expressions(expr, pin)
        if connected:
            print(f"  {pin}  ← CONNECTED to {connected.__class__.__name__}")
        else:
            print(f"  {pin}  ← UNCONNECTED (using default/const)")
