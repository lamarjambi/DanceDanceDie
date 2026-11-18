import unreal
# Headless wrapper for dump_perf.py (run by Scripts/dump_perf_cmd.bat, no editor UI needed). Dumps city + club.
for MAP, SPLIT_Y in (('/Game/NightCityCreator/maps/NightCityCreator', 1000), ('/Game/CyberClub/DemoMap/DemoMap', None)):
    exec(open(unreal.Paths.project_dir() + 'Scripts/dump_perf.py').read())
