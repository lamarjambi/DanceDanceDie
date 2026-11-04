@echo off
rem waits for editor to close, rebuilds C++, reopens project. NO git. log: Saved\build.log
cd /d "%~dp0.."
:wait
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul && (timeout /t 2 >nul & goto wait)
call "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" DanceDanceDieEditor Win64 Development -Project="%cd%\DanceDanceDie.uproject" -WaitMutex > Saved\build.log 2>&1
if errorlevel 1 (echo BUILD FAILED >> Saved\build.log & start notepad Saved\build.log & exit /b 1)
echo BUILD OK >> Saved\build.log
start "" "%cd%\DanceDanceDie.uproject"
