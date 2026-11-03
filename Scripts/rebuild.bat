@echo off
rem waits for editor to close, rebuilds C++, commits, reopens project. log: Saved\rebuild.log
cd /d "%~dp0.."
:wait
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | find /I "UnrealEditor.exe" >nul && (timeout /t 2 >nul & goto wait)
call "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" DanceDanceDieEditor Win64 Development -Project="%cd%\DanceDanceDie.uproject" -WaitMutex > Saved\rebuild.log 2>&1
if errorlevel 1 (echo BUILD FAILED >> Saved\rebuild.log & start notepad Saved\rebuild.log & exit /b 1)
git add Source Config Scripts Content/UI >> Saved\rebuild.log 2>&1
if exist Saved\commit_msg.txt (git commit -F Saved\commit_msg.txt >> Saved\rebuild.log 2>&1 & del Saved\commit_msg.txt)
echo BUILD OK >> Saved\rebuild.log
start "" "%cd%\DanceDanceDie.uproject"
