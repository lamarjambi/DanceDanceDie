import unreal
out=[]
L=unreal.EditorAssetLibrary
for p in L.list_assets('/Game/Rhythm/Material', recursive=True):
    a=L.load_asset(p); out.append('== %s %s'%(p, type(a).__name__))
    if isinstance(a, unreal.MaterialInstanceConstant):
        out.append('  parent: %s'%a.get_editor_property('parent').get_path_name())
        for v in a.get_editor_property('vector_parameter_values'): out.append('  V %s = %s'%(v.parameter_info.name, v.parameter_value))
        for v in a.get_editor_property('scalar_parameter_values'): out.append('  S %s = %s'%(v.parameter_info.name, v.parameter_value))
        for v in a.get_editor_property('texture_parameter_values'): out.append('  T %s = %s'%(v.parameter_info.name, v.parameter_value.get_path_name() if v.parameter_value else None))
    if isinstance(a, unreal.Material):
        out.append('  blend %s shading %s'%(a.get_editor_property('blend_mode'), a.get_editor_property('shading_model')))
        for n in ['vector','scalar','texture']:
            fn=getattr(unreal.MaterialEditingLibrary,'get_%s_parameter_names'%n, None)
            if fn: out.append('  %s params: %s'%(n, [str(x) for x in fn(a)]))
open(unreal.Paths.project_dir()+'Saved/mat_dump.txt','w').write('\n'.join(out))
unreal.log_warning('MATDUMP done')
