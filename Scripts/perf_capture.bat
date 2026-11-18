@echo off
rem Standalone perf capture (wrapper for perf_capture.ps1, see its header). NO git.
rem   Scripts\perf_capture.bat -Map /Game/NightCityCreator/maps/NightCityCreator -Label city_base
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0perf_capture.ps1" %*
