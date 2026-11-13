import unreal, subprocess
# starts Scripts/build_only.bat detached (it waits for the editor to close, builds C++, reopens the project).
# run it, then close the editor: exec(open(unreal.Paths.project_dir()+'Scripts/run_build.py').read())
d = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
subprocess.Popen(['cmd', '/c', d.replace('/', '\\') + 'Scripts\\build_only.bat'], cwd=d, creationflags=0x00000010, close_fds=True)
unreal.log_warning('BUILD queued: close the editor now')
