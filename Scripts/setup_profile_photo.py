import unreal
# /Game/UI/M_UI_ProfilePhoto: UI material for the live photos on the profile screen (ProfileSetup.cpp).
# a scene capture writes alpha 0, so a plain UImage draws it invisible: this one is opaque and ignores alpha.
# texture param "Photo" (the render target), slightly pixelated (PIXELS_X x PIXELS_Y blocks) to match the PS1 look.
# safe to re-run (rebuilds the graph in place):
#   exec(open(unreal.Paths.project_dir()+'Scripts/setup_profile_photo.py').read())
PATH = '/Game/UI/M_UI_ProfilePhoto'
PIXELS_X, PIXELS_Y = 96, 120
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
log = lambda s: unreal.log_warning('PHOTOMAT ' + s)

if EAL.does_asset_exist(PATH):
    mat = unreal.load_asset(PATH)
    MEL.delete_all_material_expressions(mat)
else:
    folder, name = PATH.rsplit('/', 1)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
assert mat, 'could not make ' + PATH
mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)

uv = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
grid = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant2Vector, -900, 150)
grid.set_editor_property('r', PIXELS_X); grid.set_editor_property('g', PIXELS_Y)
mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -700, 50)
flo = MEL.create_material_expression(mat, unreal.MaterialExpressionFloor, -550, 50)
half = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -420, 50)
half.set_editor_property('const_b', 0.5)
div = MEL.create_material_expression(mat, unreal.MaterialExpressionDivide, -280, 50)
tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -100, 0)
tex.set_editor_property('parameter_name', 'Photo')
tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/Black'))
tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

assert MEL.connect_material_expressions(uv, '', mul, 'A')
assert MEL.connect_material_expressions(grid, '', mul, 'B')
assert MEL.connect_material_expressions(mul, '', flo, '')
assert MEL.connect_material_expressions(flo, '', half, 'A')
assert MEL.connect_material_expressions(half, '', div, 'A')
assert MEL.connect_material_expressions(grid, '', div, 'B')
assert MEL.connect_material_expressions(div, '', tex, 'UVs')
assert MEL.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)

MEL.recompile_material(mat)
assert EAL.save_asset(PATH, only_if_is_dirty=False)
log('ok: %s (%d x %d blocks)' % (PATH, PIXELS_X, PIXELS_Y))
