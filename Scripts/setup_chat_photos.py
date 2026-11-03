# chat photos: small + nearest filter so they match the PS1 look
import unreal
for name in ['ava-text', 'bar-text', 'mateo-text']:
    path = '/Game/UI/Images/' + name
    t = unreal.load_asset(path)
    assert t, 'missing ' + path
    t.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    t.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
    t.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    t.set_editor_property('max_texture_size', 160)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    assert unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False), 'save failed ' + path
print('CHAT_PHOTOS_OK')
