# M_PS1Horror: Stencil (SceneTexture_104, CustomStencil) had no UV -> sampled at full-res pixels while Col uses the
# 180-row grid (PixelUV2). Mask and colour didn't line up -> every arrow showed twice (dim graded + bright boosted).
# Fix: feed the stencil the same PixelUV2 so both read the same block.
import unreal
PATH='/Game/PostProcess/M_PS1Horror'
mat=unreal.load_asset(PATH); MEL=unreal.MaterialEditingLibrary
uv=unreal.find_object(None,PATH+'.M_PS1Horror:MaterialExpressionCustom_202')
st=unreal.find_object(None,PATH+'.M_PS1Horror:MaterialExpressionSceneTexture_104')
assert uv and st, 'PixelUV2 or Stencil node not found'
assert st.get_editor_property('scene_texture_id')==unreal.SceneTextureId.PPI_CUSTOM_STENCIL, 'node 104 is not CustomStencil'
assert MEL.connect_material_expressions(uv,'',st,'UVs'), 'connect failed'
MEL.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False), 'save failed'
unreal.log('STENCILFIX ok')
