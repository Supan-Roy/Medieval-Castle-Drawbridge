@echo off
REM ============================================================
REM  build.bat - Smart Medieval Castle Drawbridge Simulation
REM  Compiles the whole modular C++ project into castle_sim.exe
REM  and runs it. Invoked by VS Code Ctrl+Shift+B (see .vscode).
REM ============================================================
setlocal
cd /d "%~dp0"

REM Single-file build: the whole project now lives in main.cpp.
g++ main.cpp ^
    -I. -o castle_sim.exe -lfreeglut -lopengl32 -lglu32 -mwindows

if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

echo Build OK.
echo Running castle_sim.exe ...

REM Make sure MinGW runtime DLLs (libgcc_s_dw2-1.dll, etc.) and
REM freeglut.dll are found even if they are not on the system PATH.
if exist "C:\MinGW\bin" set "PATH=C:\MinGW\bin;%PATH%"
if exist "%~dp0" set "PATH=%~dp0;%PATH%"

start castle_sim.exe
