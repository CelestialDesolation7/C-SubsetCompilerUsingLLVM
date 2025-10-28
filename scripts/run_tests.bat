@echo off
setlocal enabledelayedexpansion

echo Building ToyC compiler...
g++ -std=c++20 -O2 -Wall -o toyc.exe src\*.cpp
if errorlevel 1 (
    echo Error: Failed to build compiler
    exit /b 1
)

echo.
echo Running tests...

REM Check if scripts exist
if not exist "scripts\generate_asm.sh" (
    echo Error: scripts\generate_asm.sh not found
    exit /b 1
)

if not exist "scripts\generate_ir.sh" (
    echo Error: scripts\generate_ir.sh not found
    exit /b 1
)

REM Run tests using WSL bash
echo Running assembly generation tests...
wsl bash scripts/generate_asm.sh
if errorlevel 1 (
    echo Error: Assembly generation failed
    exit /b 1
)

echo Running IR generation tests...
wsl bash scripts/generate_ir.sh
if errorlevel 1 (
    echo Error: IR generation failed
    exit /b 1
)

echo.
echo All tests completed successfully!
