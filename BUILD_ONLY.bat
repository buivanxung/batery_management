@echo off
REM Build Project Helper
REM Chỉ build firmware (không flash)

cd /d "%~dp0"

py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PY_CMD=py -3"
) else (
    set "PY_CMD=python"
)

%PY_CMD% -m pip install --upgrade platformio >nul 2>&1

REM Get number of motors from user
echo ========================================
echo   FLASH BOARD - BUILD FIRMWARE ONLY
echo ========================================
echo.

set /p motors="Nhập số motor (6 hoặc 8, mặc định 8): "
if "%motors%"=="" set motors=8

echo.
echo Đang build firmware cho %motors% motors...
echo.

%PY_CMD% -m platformio run -e nucleo_g070rb_%motors%motor

if errorlevel 1 (
    echo.
    echo ❌ Build thất bại
    pause
) else (
    echo.
    echo ✓ Build thành công!
)

pause
