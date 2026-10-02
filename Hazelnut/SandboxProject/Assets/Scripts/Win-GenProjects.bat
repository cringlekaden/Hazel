@echo off
cd /d "%~dp0"
premake5 vs2022
if errorlevel 1 exit /b 1
