import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
hits = [a for a in ar.get_assets_by_path('/Game', recursive=True) if 'IntroChatGameMode' in str(a.asset_name)]
out = []
for a in hits:
    bp = unreal.load_asset(str(a.package_name))
    cdo = unreal.get_default_object(bp.generated_class())
    out.append('ASSET ' + str(a.package_name))
    for i, m in enumerate(cdo.get_editor_property('messages')):
        out.append('%d|%s|%s|%.2f|%.2f|%s' % (i, m.sender, m.body.replace('\n','\\n'), m.typing_time, m.hold_time, m.creepy))
open(unreal.Paths.project_dir() + 'Saved/introchat_dump.txt', 'w', encoding='utf-8').write('\n'.join(out))
print('DUMPED', len(out))
