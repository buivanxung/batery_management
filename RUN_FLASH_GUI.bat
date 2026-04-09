@echo off
setlocal
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
REM Flash Board GUI Tool - Windows Launcher
REM Double-click ????? ch???y GUI tool

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
    echo [ERROR] Khong tim thay Python
    echo.
    echo Vui long cai Python 3.9+ tu: https://www.python.org/downloads/
    echo Nho tick "Add Python to PATH" khi cai dat.
    echo.
    pause
    exit /b 1
)

REM Create and use local virtual environment for reproducible setup
set "VENV_DIR=%~dp0.venv"
set "VENV_PY=%VENV_DIR%\Scripts\python.exe"

if not exist "%VENV_PY%" (
    echo.
    echo [INFO] Tao virtual environment...
    %PY_CMD% -m venv "%VENV_DIR%"
    if errorlevel 1 (
        echo [ERROR] Khong tao duoc virtual environment
        pause
        exit /b 1
    )
)

echo.
echo [INFO] Cai/Update dependency (pyserial, platformio)...
"%VENV_PY%" -m pip install --upgrade pip --quiet
"%VENV_PY%" -m pip install --upgrade pyserial platformio
if errorlevel 1 (
    echo [ERROR] Khong the cai dependency Python
    pause
    exit /b 1
)

REM Check STM32CubeProgrammer CLI for auto-unlock support
set "CUBEPROG_CLI="
set "CUBEPROG_INSTALLER="

for %%I in ("%~dp0app\*CubeProgrammer*.exe") do (
    if not defined CUBEPROG_INSTALLER set "CUBEPROG_INSTALLER=%%~fI"
)

if "%CUBEPROG_INSTALLER%"=="" (
    if exist "%~dp0app\SetupSTM32CubeProgrammer_win64.exe" (
        set "CUBEPROG_INSTALLER=%~dp0app\SetupSTM32CubeProgrammer_win64.exe"
    )
)

call :find_cubeprog_cli

if "%CUBEPROG_CLI%"=="" (
    echo.
    if not "%CUBEPROG_INSTALLER%"=="" (
        echo [INFO] Khong tim thay STM32CubeProgrammer CLI. Thu cai dat tu app folder...
        echo [INFO] Installer: %CUBEPROG_INSTALLER%
        "%CUBEPROG_INSTALLER%" /S >nul 2>&1
        if errorlevel 1 (
            echo [WARN] Silent install khong thanh cong, thu chay installer mac dinh...
            start /wait "" "%CUBEPROG_INSTALLER%"
        )
    ) else (
        echo [INFO] Khong tim thay installer trong app folder.
    )

    REM Re-check after local installation attempt
    call :find_cubeprog_cli

    if "%CUBEPROG_CLI%"=="" (
        echo [INFO] CLI van chua co. Thu cai dat bang winget...
        winget --version >nul 2>&1
        if errorlevel 1 (
            echo [WARN] Khong co winget tren may nay. Tinh nang auto-unlock chip co the khong hoat dong.
        ) else (
            winget install --id STMicroelectronics.STM32CubeProgrammer --source winget --accept-source-agreements --accept-package-agreements --silent
            if errorlevel 1 (
                echo [WARN] Khong the cai STM32CubeProgrammer bang winget.
                echo [WARN] Ban co the cai tay tu: https://www.st.com/en/development-tools/stm32cubeprog.html
            ) else (
                echo [OK] Da cai STM32CubeProgrammer bang winget.
            )
        )

        REM Re-check after winget installation attempt
        call :find_cubeprog_cli
    )
)

if "%CUBEPROG_CLI%"=="" (
    echo [WARN] STM32CubeProgrammer CLI chua san sang. GUI van chay, nhung auto-unlock chip co the khong dung duoc.
) else (
    echo [OK] STM32CubeProgrammer CLI san sang: %CUBEPROG_CLI%
)

REM tkinter must be available from Python installation
"%VENV_PY%" -c "import tkinter" >nul 2>&1
if errorlevel 1 (
    echo.
    echo [ERROR] Python hien tai khong co tkinter
    echo Hay cai lai Python ban chinh thuc tu python.org - co Tcl/Tk.
    echo.
    pause
    exit /b 1
)

REM Run the GUI
echo.
echo [OK] Khoi dong Flash Board GUI...
echo.

"%VENV_PY%" flash_board_gui.py

if errorlevel 1 (
    echo.
    echo [ERROR] Loi khi chay GUI tool
    pause
    exit /b 1
)

exit /b 0

:find_cubeprog_cli
set "CUBEPROG_CLI="

if defined STM32_PROGRAMMER_CLI (
    if exist "%STM32_PROGRAMMER_CLI%" set "CUBEPROG_CLI=%STM32_PROGRAMMER_CLI%"
)

if "%CUBEPROG_CLI%"=="" (
    where /q STM32_Programmer_CLI.exe
    if not errorlevel 1 set "CUBEPROG_CLI=STM32_Programmer_CLI.exe"
)

if "%CUBEPROG_CLI%"=="" (
    for %%P in (
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
        "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
        "C:\Program Files\STMicroelectronics\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
        "C:\Program Files (x86)\STMicroelectronics\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    ) do (
        if exist "%%~P" (
            set "CUBEPROG_CLI=%%~P"
            goto :find_cubeprog_done
        )
    )
)

if "%CUBEPROG_CLI%"=="" (
    for /f "delims=" %%F in ('dir /b /s "C:\Program Files\*STM32_Programmer_CLI.exe" 2^>nul') do (
        set "CUBEPROG_CLI=%%~fF"
        goto :find_cubeprog_done
    )
)

if "%CUBEPROG_CLI%"=="" (
    for /f "delims=" %%F in ('dir /b /s "C:\Program Files (x86)\*STM32_Programmer_CLI.exe" 2^>nul') do (
        set "CUBEPROG_CLI=%%~fF"
        goto :find_cubeprog_done
    )
)

:find_cubeprog_done
exit /b 0
