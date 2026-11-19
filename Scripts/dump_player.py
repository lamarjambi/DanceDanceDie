import unreal
# Dumps the player pawn's components (camera, boom, mesh) + head/neck ref pose -> Saved/player_dump.txt
# Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this>
BP = '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'
out = []
def P(*a): out.append(' '.join(str(x) for x in a))

bp = unreal.load_asset(BP)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
mesh_asset = None
for h in sds.k2_gather_subobject_data_for_blueprint(bp):
    d = sds.k2_find_subobject_data_from_handle(h)
    o = lib.get_object(d)
    if not o or not isinstance(o, unreal.ActorComponent):
        continue
    P('---', o.get_name(), o.get_class().get_name())
    if isinstance(o, unreal.SceneComponent):
        par = o.get_attach_parent()
        P('  parent', par.get_name() if par else None, 'socket', o.get_attach_socket_name())
        P('  loc', o.relative_location, 'rot', o.relative_rotation, 'scale', o.relative_scale3d)
    props = []
    if isinstance(o, unreal.CameraComponent):
        props = ['field_of_view', 'use_pawn_control_rotation', 'constrain_aspect_ratio', 'post_process_blend_weight']
    elif isinstance(o, unreal.SpringArmComponent):
        props = ['target_arm_length', 'socket_offset', 'target_offset', 'use_pawn_control_rotation', 'do_collision_test',
                 'enable_camera_lag', 'enable_camera_rotation_lag', 'inherit_pitch', 'inherit_yaw', 'inherit_roll', 'probe_size']
    elif isinstance(o, unreal.CapsuleComponent):
        props = ['capsule_half_height', 'capsule_radius']
    elif isinstance(o, unreal.SkeletalMeshComponent):
        props = ['owner_no_see', 'cast_shadow', 'cast_hidden_shadow', 'bounds_scale', 'visibility_based_anim_tick_option']
        mesh_asset = o.get_skeletal_mesh_asset()
        P('  mesh', mesh_asset.get_path_name() if mesh_asset else None, 'anim', o.anim_class)
        for i, m in enumerate(o.get_materials()):
            P('  override mat', i, m.get_path_name() if m else None)
    elif isinstance(o, unreal.CharacterMovementComponent):
        props = ['max_walk_speed', 'orient_rotation_to_movement', 'use_controller_desired_rotation']
    for p in props:
        try: P('  ', p, '=', o.get_editor_property(p))
        except Exception as e: P('  ', p, 'ERR', e)

cdo = unreal.get_default_object(bp.generated_class())
for p in ['use_controller_rotation_yaw', 'use_controller_rotation_pitch', 'base_eye_height', 'crouched_eye_height']:
    try: P('pawn', p, '=', cdo.get_editor_property(p))
    except Exception as e: P('pawn', p, 'ERR', e)

if mesh_asset:
    P('=== mesh', mesh_asset.get_name(), 'bounds', mesh_asset.get_bounds())
    for i, sm in enumerate(mesh_asset.materials):
        mi = sm.material_interface
        base = mi.get_base_material() if mi else None
        P('  slot', i, sm.material_slot_name, mi.get_path_name() if mi else None,
          'two_sided', base.get_editor_property('two_sided') if base else None,
          'blend', base.get_editor_property('blend_mode') if base else None)
    skel = mesh_asset.skeleton
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
        for b in unreal.AnimPoseExtensions.get_bone_names(pose):
            s = str(b)
            if any(k in s for k in ('Head', 'Neck', 'Spine', 'Hips', 'Shoulder', 'Eye')):
                t = unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD)
                P('  bone', s, 'loc', t.translation, 'rot', t.rotation.rotator())
    except Exception as e:
        P('pose ERR', e)

open(unreal.Paths.project_saved_dir() + 'player_dump.txt', 'w').write('\n'.join(out))
unreal.log('player dump done')
