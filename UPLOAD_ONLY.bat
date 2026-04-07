@echo off
REM Upload Firmware to Board
REM Upload lên board qua ST-Link

cd /d "%~dp0"

py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PY_CMD=py -3"
) else (
    set "PY_CMD=python"
)

%PY_CMD% -m pip install --upgrade platformio >nul 2>&1

echo ========================================
echo   FLASH BOARD - UPLOAD FIRMWARE
echo ========================================
echo.

set /p motors="Nhập số motor (6 hoặc 8, mặc định 8): "
if "%motors%"=="" set motors=8

set /p port="Nhập COM port (ví dụ: COM3): "
if "%port%"=="" set port=COM3

echo.
echo Đang upload firmware to board (%motors% motors, port: %port%)...
echo.

%PY_CMD% flash_board.py --motors %motors% --port %port% --skip-audio

if errorlevel 1 (
    echo.
    echo ❌ Upload thất bại
    echo.
    echo Kiểm tra:
    echo 1. Board có cắm vào USB không?
    echo 2. COM port có đúng không? (xem Device Manager)
    echo 3. ST-Link driver đã cài chưa?
    pause
) else (
    echo.
    echo ✓ Upload thành công!
    echo.
    echo Board sẽ khởi động lại
)

pause
