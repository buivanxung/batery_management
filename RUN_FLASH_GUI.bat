@echo off
REM Flash Board GUI Tool - Windows Launcher
REM Double-click để chạy GUI tool

cd /d "%~dp0"

REM Prefer Python Launcher on Windows, fallback to python in PATH
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

if "%PY_CMD%"=="" (
    echo.
    echo ❌ Lỗi: Không tìm thấy Python
    echo.
    echo Vui lòng cài Python 3.9+ từ: https://www.python.org/downloads/
    echo Nhớ tick "Add Python to PATH" khi cài đặt.
    echo.
    pause
    exit /b 1
)

REM Create and use local virtual environment for reproducible setup
set "VENV_DIR=%~dp0.venv"
set "VENV_PY=%VENV_DIR%\Scripts\python.exe"

if not exist "%VENV_PY%" (
    echo.
    echo ⚙️  Tạo virtual environment...
    %PY_CMD% -m venv "%VENV_DIR%"
    if errorlevel 1 (
        echo ❌ Lỗi: Không tạo được virtual environment
        pause
        exit /b 1
    )
)

echo.
echo ⚙️  Cài/Update dependency (pyserial, platformio)...
"%VENV_PY%" -m pip install --upgrade pip --quiet
"%VENV_PY%" -m pip install --upgrade pyserial platformio
if errorlevel 1 (
    echo ❌ Lỗi: Không thể cài dependency Python
    pause
    exit /b 1
)

REM Check STM32CubeProgrammer CLI for auto-unlock support
set "CUBEPROG_CLI="
where /q STM32_Programmer_CLI.exe
if not errorlevel 1 set "CUBEPROG_CLI=STM32_Programmer_CLI.exe"

if "%CUBEPROG_CLI%"=="" (
    if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
        set "CUBEPROG_CLI=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
)

if "%CUBEPROG_CLI%"=="" (
    if exist "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
        set "CUBEPROG_CLI=C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
)

if "%CUBEPROG_CLI%"=="" (
    echo.
    echo ⚙️  Khong tim thay STM32CubeProgrammer CLI. Thu cai dat bang winget...
    winget --version >nul 2>&1
    if errorlevel 1 (
        echo ⚠️  Khong co winget tren may nay. Tinh nang auto-unlock chip co the khong hoat dong.
    ) else (
        winget install --id STMicroelectronics.STM32CubeProgrammer --source winget --accept-source-agreements --accept-package-agreements --silent
        if errorlevel 1 (
            echo ⚠️  Khong the cai STM32CubeProgrammer bang winget.
            echo ⚠️  Ban co the cai tay tu: https://www.st.com/en/development-tools/stm32cubeprog.html
        ) else (
            echo ✓ Da cai STM32CubeProgrammer bang winget.
        )
    )

    REM Re-check after installation attempt
    where /q STM32_Programmer_CLI.exe
    if not errorlevel 1 set "CUBEPROG_CLI=STM32_Programmer_CLI.exe"
    if "%CUBEPROG_CLI%"=="" (
        if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
            set "CUBEPROG_CLI=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
        )
    )
    if "%CUBEPROG_CLI%"=="" (
        if exist "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" (
            set "CUBEPROG_CLI=C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
        )
    )
)

if "%CUBEPROG_CLI%"=="" (
    echo ⚠️  STM32CubeProgrammer CLI chua san sang. GUI van chay, nhung auto-unlock chip co the khong dung duoc.
) else (
    echo ✓ STM32CubeProgrammer CLI san sang: %CUBEPROG_CLI%
)

REM tkinter must be available from Python installation
"%VENV_PY%" -c "import tkinter" >nul 2>&1
if errorlevel 1 (
    echo.
    echo ❌ Python hiện tại không có tkinter
    echo Hãy cài lại Python bản chính thức từ python.org - co Tcl/Tk.
    echo.
    pause
    exit /b 1
)

REM Run the GUI
echo.
echo ✓ Khởi động Flash Board GUI...
echo.

"%VENV_PY%" flash_board_gui.py

if errorlevel 1 (
    echo.
    echo ❌ Lỗi khi chạy GUI tool
    pause
    exit /b 1
)
