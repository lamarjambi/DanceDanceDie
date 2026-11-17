# Dumps BP_Spline instances (components + world transforms) and BP_Note1 CDO components -> Saved/lanes_dump.txt
import unreal
o=[]
w=unreal.EditorLevelLibrary.get_editor_world(); o.append('world %s'%w.get_name())
def comps(a):
    for c in a.get_components_by_class(unreal.SceneComponent):
        t=c.get_world_transform(); line='  %s [%s] loc=%s rot=%s scale=%s'%(c.get_name(),c.get_class().get_name(),t.translation,t.rotation.rotator(),t.scale3d)
        if isinstance(c,unreal.StaticMeshComponent):
            line+=' mesh=%s mat=%s'%(c.static_mesh.get_name() if c.static_mesh else None,[m.get_name() if m else None for m in c.get_materials()])
        if isinstance(c,unreal.SplineComponent):
            n=c.get_number_of_spline_points()
            line+=' pts='+'; '.join('%s'%c.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.WORLD) for i in range(n))
        o.append(line)
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    cn=a.get_class().get_name()
    if 'Spline' in cn or 'Note' in cn or 'Target' in a.get_actor_label() or 'Camera' in cn:
        o.append('ACTOR %s (%s)'%(a.get_actor_label(),cn)); comps(a)
bp=unreal.load_asset('/Game/Rhythm/BP_Note1'); cdo=unreal.get_default_object(bp.generated_class())
o.append('BP_Note1 CDO:')
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    d=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h); obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
    if isinstance(obj,unreal.SceneComponent):
        o.append('  %s [%s] relrot=%s relloc=%s relscale=%s'%(obj.get_name(),obj.get_class().get_name(),obj.get_editor_property('relative_rotation'),obj.get_editor_property('relative_location'),obj.get_editor_property('relative_scale3d')))
open(unreal.Paths.project_dir()+'Saved/lanes_dump.txt','w').write('\n'.join(o)); unreal.log('LANES ok')
