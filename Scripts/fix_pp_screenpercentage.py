# Makes M_PS1Horror work with r.ScreenPercentage < 100.
# Bug: PixelUV2 returned a *viewport* UV, but SceneTexture Coordinates expects a *buffer* UV.
# When the render buffer is bigger than the view rect (low screen %), the image only filled a corner.
import unreal
PATH = '/Game/PostProcess/M_PS1Horror'
mat = unreal.load_asset(PATH)
node = unreal.find_object(None, PATH + '.M_PS1Horror:MaterialExpressionCustom_202')
assert node, 'MaterialExpressionCustom_202 (PixelUV2) not found'
node.set_editor_property('code',
    'float rows=180; '
    'float2 g=float2(rows*View.ViewSizeAndInvSize.x*View.ViewSizeAndInvSize.w,rows); '
    'float2 vp=(floor(UV*g)+0.5)/g; '
    'return (vp*View.ViewSizeAndInvSize.xy+View.ViewRectMin.xy)*View.BufferSizeAndInvSize.zw;')
unreal.MaterialEditingLibrary.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False), 'save failed'
open(unreal.Paths.project_dir() + 'Saved/pp_fix.txt', 'w').write('ok')
