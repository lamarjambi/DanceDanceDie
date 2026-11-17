import unreal
# copies PLAY/QUIT text colours (WBP_MainMenu Quit_Text) onto BP_CreditsGameMode's looks. Overwrites colour/shadow only.
#   exec(open(unreal.Paths.project_dir()+'Scripts/credits_match_menu.py').read())
LIME=unreal.LinearColor(0.25,0.9,0.08,1); MAG=unreal.LinearColor(0.45,0.02,0.42,1); OFF=unreal.Vector2D(5,5)
BP='/Game/UI/BP_CreditsGameMode'
c=unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(BP))
for k,col,sh in [('title_look',LIME,MAG),('name_look',LIME,MAG),('heading_look',MAG,unreal.LinearColor(0,0,0,0))]:
  l=c.get_editor_property(k); l.set_editor_property('color',col); l.set_editor_property('shadow_color',sh); l.set_editor_property('shadow_offset',OFF)
  c.set_editor_property(k,l)
unreal.EditorAssetLibrary.save_asset(BP,False)
unreal.log_warning('CREDITS match: '+str(c.get_editor_property('name_look').get_editor_property('color')))
