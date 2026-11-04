# Copies Scripts/t3d/<name> to the Windows clipboard so it can be Ctrl+V'd into a Blueprint graph.
# Usage (editor Python/Cmd box):  py exec(open(unreal.Paths.project_dir()+'Scripts/clip_t3d.py').read()); clip('note_new.t3d')
import unreal, subprocess
def clip(name):
    p = unreal.Paths.project_dir() + 'Scripts/t3d/' + name
    data = open(p, 'rb').read()
    subprocess.run('clip', input=data, check=True, shell=True)
    unreal.log_warning('CLIP ok %s %d bytes' % (name, len(data)))
