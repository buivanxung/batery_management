@echo off
setlocal
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
REM Flash Board GUI Tool - Windows Launcher
REM Double-click ????? ch???y GUI tool

cd /d "%~dp0"

REM Optional STM8 mode:
REM   RUN_FLASH_GUI.bat stm8
REM This runs: upload firmware.
REM   RUN_FLASH_GUI.bat stm8-unlock
REM This runs: unlock only (manual).
REM   RUN_FLASH_GUI.bat stm8-protect
REM This runs: protect only (manual).
set "STM8_NO_PAUSE=0"
if /I "%~2"=="--no-pause" set "STM8_NO_PAUSE=1"
if /I "%~1"=="stm8" goto :stm8_all
if /I "%~1"=="stm8-unlock" goto :stm8_unlock_only
if /I "%~1"=="stm8-protect" goto :stm8_protect_only

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

:stm8_all
set "STM8_DIR=%~dp0STM8_bat"
set "STM8_FLASH=%STM8_DIR%\stm8flash.exe"
set "STM8_FW=%STM8_DIR%\bin\firmware.hex"
set "STM8_PART=stm8s003f3"
set "STM8_PGM="

if not exist "%STM8_FLASH%" (
    echo.
    echo [ERROR] Khong tim thay stm8flash: %STM8_FLASH%
    call :stm8_maybe_pause
    exit /b 1
)

if not exist "%STM8_FW%" (
    echo.
    echo [ERROR] Khong tim thay firmware STM8: %STM8_FW%
    call :stm8_maybe_pause
    exit /b 1
)

call :ensure_stm8_libusb
if errorlevel 1 (
    call :stm8_maybe_pause
    exit /b 1
)

echo.
echo [STM8] Tim ST-Link programmer...
call :detect_stm8_programmer
if "%STM8_PGM%"=="" (
    echo [ERROR] Khong ket noi duoc ST-Link cho STM8
    call :stm8_maybe_pause
    exit /b 1
)
echo [STM8] Su dung programmer: %STM8_PGM%

echo.
echo [STM8] Step 1/1: Upload firmware
call :stm8_upload
if errorlevel 1 (
    echo [WARN] Upload firmware lan 1 that bai, thu unlock roi upload lai...

    echo.
    echo [STM8] - Auto unlock chip
    call :stm8_unlock
    if errorlevel 1 (
        echo [ERROR] Unlock STM8 that bai
        echo [INFO] Ban co the thu lai bang: RUN_FLASH_GUI.bat stm8-unlock
        call :stm8_maybe_pause
        exit /b 1
    )

    echo.
    echo [STM8] - Retry upload sau unlock
    call :stm8_upload
    if errorlevel 1 (
        echo [ERROR] Upload firmware STM8 that bai sau khi unlock
        call :stm8_maybe_pause
        exit /b 1
    )
)

echo.
echo [OK] Hoan tat STM8: upload
echo [INFO] Neu can mo khoa chip, chay: RUN_FLASH_GUI.bat stm8-unlock
echo [INFO] Neu can khoa chip lai, chay: RUN_FLASH_GUI.bat stm8-protect
call :stm8_maybe_pause
exit /b 0

:stm8_unlock_only
set "STM8_DIR=%~dp0STM8_bat"
set "STM8_FLASH=%STM8_DIR%\stm8flash.exe"
set "STM8_PART=stm8s003f3"
set "STM8_PGM="

if not exist "%STM8_FLASH%" (
    echo.
    echo [ERROR] Khong tim thay stm8flash: %STM8_FLASH%
    call :stm8_maybe_pause
    exit /b 1
)

call :ensure_stm8_libusb
if errorlevel 1 (
    call :stm8_maybe_pause
    exit /b 1
)

echo.
echo [STM8] Tim ST-Link programmer...
call :detect_stm8_programmer
if "%STM8_PGM%"=="" (
    echo [ERROR] Khong ket noi duoc ST-Link cho STM8
    call :stm8_maybe_pause
    exit /b 1
)
echo [STM8] Su dung programmer: %STM8_PGM%

echo.
echo [STM8] Unlock chip...
call :stm8_unlock
if errorlevel 1 (
    echo [ERROR] Unlock STM8 that bai
    call :stm8_maybe_pause
    exit /b 1
)

echo.
echo [OK] STM8 da duoc unlock
call :stm8_maybe_pause
exit /b 0

:stm8_protect_only
set "STM8_DIR=%~dp0STM8_bat"
set "STM8_FLASH=%STM8_DIR%\stm8flash.exe"
set "STM8_PART=stm8s003f3"
set "STM8_PGM="

if not exist "%STM8_FLASH%" (
    echo.
    echo [ERROR] Khong tim thay stm8flash: %STM8_FLASH%
    call :stm8_maybe_pause
    exit /b 1
)

call :ensure_stm8_libusb
if errorlevel 1 (
    call :stm8_maybe_pause
    exit /b 1
)

echo.
echo [STM8] Tim ST-Link programmer...
call :detect_stm8_programmer
if "%STM8_PGM%"=="" (
    echo [ERROR] Khong ket noi duoc ST-Link cho STM8
    call :stm8_maybe_pause
    exit /b 1
)
echo [STM8] Su dung programmer: %STM8_PGM%

echo.
echo [STM8] Protect chip...
call :stm8_protect
if errorlevel 1 (
    echo [ERROR] Protect STM8 that bai
    call :stm8_maybe_pause
    exit /b 1
)

echo.
echo [OK] STM8 da duoc protect
call :stm8_maybe_pause
exit /b 0

:stm8_maybe_pause
if "%STM8_NO_PAUSE%"=="1" exit /b 0
pause
exit /b 0

:detect_stm8_programmer
set "STM8_PGM="

for %%P in (stlinkv2 stlink stlinkv21 stlinkv3) do (
    "%STM8_FLASH%" -c %%P -p %STM8_PART% >nul 2>&1
    if not errorlevel 1 (
        set "STM8_PGM=%%P"
        goto :detect_stm8_programmer_done
    )
)

:detect_stm8_programmer_done
exit /b 0

:stm8_unlock
call :run_stm8_checked "%STM8_FLASH%" -c %STM8_PGM% -p %STM8_PART% -u
exit /b %errorlevel%

:stm8_upload
echo [STM8] - Upload firmware (write)
call :run_stm8_checked "%STM8_FLASH%" -c %STM8_PGM% -p %STM8_PART% -w "%STM8_FW%"
exit /b %errorlevel%

:stm8_protect
set "STM8_TMP_PROTECT=%TEMP%\stm8_protect_rop.hex"
>"%STM8_TMP_PROTECT%" echo :0248000000FFB7
>>"%STM8_TMP_PROTECT%" echo :00000001FF

call :run_stm8_checked "%STM8_FLASH%" -c %STM8_PGM% -p %STM8_PART% -s opt -w "%STM8_TMP_PROTECT%"
set "STM8_RET=%errorlevel%"
del /f /q "%STM8_TMP_PROTECT%" >nul 2>&1
exit /b %STM8_RET%

:run_stm8_checked
set "STM8_LOG=%TEMP%\stm8flash_run_%RANDOM%%RANDOM%.log"
%* >"%STM8_LOG%" 2>&1
set "STM8_RET=%errorlevel%"

type "%STM8_LOG%"

findstr /i /c:"Tries exceeded" /c:"FAILED" /c:"Could not open USB device" /c:"Couldn't initialize stlink" /c:"Error communicating with MCU" "%STM8_LOG%" >nul
if not errorlevel 1 set "STM8_RET=1"

del /f /q "%STM8_LOG%" >nul 2>&1
exit /b %STM8_RET%

:ensure_stm8_libusb
set "STM8_LIBUSB=%STM8_DIR%\libusb-1.0.dll"

if exist "%STM8_LIBUSB%" exit /b 0

echo.
echo [STM8] Thieu libusb-1.0.dll, dang thu bo sung tu may...

for %%D in (
    "%~dp0app\libusb-1.0.dll"
    "C:\Windows\System32\libusb-1.0.dll"
    "C:\Windows\SysWOW64\libusb-1.0.dll"
    "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\libusb-1.0.dll"
    "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\libusb-1.0.dll"
    "C:\Program Files\STMicroelectronics\STM32CubeProgrammer\bin\libusb-1.0.dll"
    "C:\Program Files (x86)\STMicroelectronics\STM32CubeProgrammer\bin\libusb-1.0.dll"
) do (
    if exist "%%~D" (
        copy /y "%%~D" "%STM8_LIBUSB%" >nul 2>&1
        if exist "%STM8_LIBUSB%" (
            echo [OK] Da bo sung libusb-1.0.dll cho STM8 tool
            exit /b 0
        )
    )
)

echo [ERROR] stm8flash.exe can libusb-1.0.dll nhung chua tim thay tren may
echo [INFO] Cach sua nhanh:
echo [INFO] 1) Cai STM32CubeProgrammer (de co libusb-1.0.dll)
echo [INFO] 2) Copy libusb-1.0.dll vao: %STM8_DIR%
echo [INFO] 3) Chay lai RUN_FLASH_GUI.bat stm8
exit /b 1

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
