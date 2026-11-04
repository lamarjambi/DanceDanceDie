import unreal
MEL=unreal.MaterialEditingLibrary; EAL=unreal.EditorAssetLibrary
o=[]
m=EAL.load_asset('/Game/Rhythm/Material/M_Icon')
o.append('Glow default %s'%MEL.get_material_default_scalar_parameter_value(m,'Glow'))
for p in [unreal.MaterialProperty.MP_EMISSIVE_COLOR, unreal.MaterialProperty.MP_OPACITY_MASK, unreal.MaterialProperty.MP_BASE_COLOR]:
    n=MEL.get_material_property_input_node(m,p); o.append('%s <- %s'%(p, n.get_name() if n else None))
for e in MEL.get_material_expressions(m) if hasattr(MEL,'get_material_expressions') else []:
    o.append('expr %s'%e.get_name())
for mi in ['MI_Left','MI_Down','MI_Up','MI_Right']:
    i=EAL.load_asset('/Game/Rhythm/Material/'+mi)
    o.append('%s Glow=%s'%(mi, MEL.get_material_instance_scalar_parameter_value(i,'Glow')))
t=EAL.load_asset('/Game/Rhythm/UI/arrowRight')
o.append('tex %dx%d'%(t.blueprint_get_size_x(),t.blueprint_get_size_y()))
task=unreal.AssetExportTask(); task.object=t; task.filename=unreal.Paths.project_dir()+'Saved/arrowRight.png'; task.automated=True; task.replace_identical=True; task.prompt=False
task.exporter=unreal.TextureExporterPNG()
o.append('export %s'%unreal.Exporter.run_asset_export_task(task))
open(unreal.Paths.project_dir()+'Saved/icon_dump.txt','w').write('\n'.join(o))
