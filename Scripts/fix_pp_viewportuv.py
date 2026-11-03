# M_PS1Horror PixelUV2: map viewport UV -> buffer UV with the engine helper instead of View.* math.
# The View.* version was right in one pass setup but wrong in PIE at r.ScreenPercentage 60:
# the image came out zoomed 1/0.6 from the top-left, so world and UI (E prompt) no longer lined up.
# ViewportUVToSceneTextureUV(uv, 14) uses PostProcessInput0's own viewport/buffer sizes, so it's right in every pass.
import unreal
PATH = '/Game/PostProcess/M_PS1Horror'
mat = unreal.load_asset(PATH)
node = unreal.find_object(None, PATH + '.M_PS1Horror:MaterialExpressionCustom_202')
assert node, 'MaterialExpressionCustom_202 (PixelUV2) not found'
node.set_editor_property('code',
    'float rows=180; '
    'float2 g=float2(rows*View.ViewSizeAndInvSize.x*View.ViewSizeAndInvSize.w,rows); '
    'float2 vp=(floor(UV*g)+0.5)/g; '
    'return ViewportUVToSceneTextureUV(vp, 14);')
unreal.MaterialEditingLibrary.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False), 'save failed'
unreal.log('PPFIX ok')
