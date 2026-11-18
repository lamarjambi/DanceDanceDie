@echo off
rem Headless level audit (lights/meshes) -> Saved\perf_dump_<map>.txt. Works with the editor open. NO git. Saves nothing.
cd /d "%~dp0.."
"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%cd%\DanceDanceDie.uproject" -run=pythonscript -script="%cd%\Scripts\dump_perf_cmd.py" -unattended -nosplash -abslog="%cd%\Saved\Logs\perf_dump.log"
