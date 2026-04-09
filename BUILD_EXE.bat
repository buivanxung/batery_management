@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
REM Build standalone EXE for Flash Board GUI
REM Tao file EXE standalone khong can cai them Python dependency

cd /d "%~dp0"

echo.
echo ============================================
echo   BUILD FLASH BOARD GUI STANDALONE EXE
echo ============================================
echo.

REM Find Python
set "PY_EXE="
set "PY_ARGS="
py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PY_EXE=py"
    set "PY_ARGS=-3"
) else (
    python --version >nul 2>&1
    if not errorlevel 1 (
        set "PY_EXE=python"
        set "PY_ARGS="
    )
)

if "!PY_EXE!"=="" (
    echo.
    echo [ERROR] Khong tim thay Python
    echo.
    echo Vui long cai Python 3.9+ tu: https://www.python.org/downloads/
    echo Nho tick "Add Python to PATH" khi cai dat.
    echo.
    pause
    exit /b 1
)

echo [INFO] Chuan bi build...
echo.

REM Run build script
if "!PY_ARGS!"=="" (
    "!PY_EXE!" build_exe.py
) else (
    "!PY_EXE!" !PY_ARGS! build_exe.py
)
set "BUILD_RESULT=!errorlevel!"

echo.
if "!BUILD_RESULT!"=="0" (
    echo ============================================
    echo [OK] BUILD THANH CONG
    echo ============================================
    echo.
    echo File EXE nam tai: dist\FlashBoard-GUI.exe
    echo.
    echo Huong dan su dung:
    echo   - Copy file: dist\FlashBoard-GUI.exe
    echo   - Paste vao thu muc bat ky tren may khac
    echo   - Double-click de chay
    echo.
) else (
    echo ============================================
    echo [ERROR] BUILD THAT BAI
    echo ============================================
    echo.
    echo Kiem tra loi phia tren.
    echo.
)

pause
exit /b !BUILD_RESULT!
