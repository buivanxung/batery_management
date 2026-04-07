@echo off
REM Flash Board GUI Tool - Windows Launcher
REM Double-click để chạy GUI tool

cd /d "%~dp0"

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo.
    echo ❌ Lỗi: Python chưa được cài hoặc không trong PATH
    echo.
    echo Vui lòng:
    echo 1. Cài Python từ https://www.python.org/downloads/
    echo 2. Tick vào "Add Python to PATH"
    echo 3. Restart Windows
    echo 4. Chạy lại file này
    echo.
    pause
    exit /b 1
)

REM Check if required packages are installed
python -c "import tkinter; import serial" 2>nul
if errorlevel 1 (
    echo.
    echo ⚠️  Cài đặt các thư viện Python cần thiết...
    echo.
    python -m pip install pyserial --quiet
    if errorlevel 1 (
        echo ❌ Lỗi: Không thể cài pyserial
        pause
        exit /b 1
    )
)

REM Run the GUI
echo.
echo ✓ Khởi động Flash Board GUI...
echo.

python flash_board_gui.py

if errorlevel 1 (
    echo.
    echo ❌ Lỗi khi chạy GUI tool
    pause
    exit /b 1
)
