@echo off
REM Build standalone .exe for Flash Board GUI
REM Tạo file .exe standalone không cần install Python hay dependency

setlocal enabledelayedexpansion

cd /d "%~dp0"

echo.
echo ============================================
echo   BUILD FLASH BOARD GUI STANDALONE .EXE
echo ============================================
echo.

REM Find Python
set "PY_CMD="
py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PY_CMD=py -3"
) else (
    python --version >nul 2>&1
    if not errorlevel 1 (
        set "PY_CMD=python"
    )
)

if "!PY_CMD!"=="" (
    echo.
    echo ❌ Lỗi: Không tìm thấy Python
    echo.
    echo Vui lòng cài Python 3.9+ từ: https://www.python.org/downloads/
    echo Nhớ tick "Add Python to PATH" khi cài đặt.
    echo.
    pause
    exit /b 1
)

echo ⚙️  Chuẩn bị build...
echo.

REM Run build script
"!PY_CMD!" build_exe.py
set BUILD_RESULT=!errorlevel!

echo.
if !BUILD_RESULT! equ 0 (
    echo ============================================
    echo ✅ BUILD THÀNH CÔNG!
    echo ============================================
    echo.
    echo File .exe nằm tại: dist\FlashBoard-GUI.exe
    echo.
    echo 📌 Hướng dẫn sử dụng:
    echo    - Copy file: dist\FlashBoard-GUI.exe
    echo    - Paste vào thư mục bất kỳ trên máy khác
    echo    - Double-click để chạy (không cần install gì)
    echo.
) else (
    echo ============================================
    echo ❌ BUILD THẤT BẠI
    echo ============================================
    echo.
    echo Kiểm tra lỗi phía trên.
    echo.
)

pause
exit /b !BUILD_RESULT!
