@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

echo ===================================================
echo Aether OS Interactive Startup Script
echo Running from: %SCRIPT_DIR%
echo ===================================================

:: 1. Find VirtualBox/VBoxManage
set "VBOX_DIR="
if exist "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" (
    set "VBOX_DIR=C:\Program Files\Oracle\VirtualBox"
) else if exist "%ProgramFiles%\Oracle\VirtualBox\VBoxManage.exe" (
    set "VBOX_DIR=%ProgramFiles%\Oracle\VirtualBox"
) else if exist "%ProgramFiles(x86)%\Oracle\VirtualBox\VBoxManage.exe" (
    set "VBOX_DIR=%ProgramFiles(x86)%\Oracle\VirtualBox"
)

if "%VBOX_DIR%"=="" (
    for /f "tokens=*" %%i in ('where VBoxManage.exe 2^>nul') do (
        set "VBOX_DIR=%%~dpi"
    )
)

if not "%VBOX_DIR%"=="" (
    if "!VBOX_DIR:~-1!"=="\" set "VBOX_DIR=!VBOX_DIR:~0,-1!"
)

if "%VBOX_DIR%"=="" (
    echo [ERROR] VirtualBox was not found in Program Files or system PATH.
    echo Please make sure VirtualBox is installed.
    pause
    exit /b 1
)

echo Found VirtualBox at: %VBOX_DIR%

:: 2. Find AetherOS-64 directory path
set "AETHEROS_64_DIR="
if exist "%SCRIPT_DIR%\AetherOS.iso" (
    set "AETHEROS_64_DIR=%SCRIPT_DIR%"
) else if exist "%SCRIPT_DIR%\AetherOS-64\AetherOS.iso" (
    set "AETHEROS_64_DIR=%SCRIPT_DIR%\AetherOS-64"
) else if exist "%SCRIPT_DIR%\..\AetherOS-64\AetherOS.iso" (
    set "AETHEROS_64_DIR=%SCRIPT_DIR%\..\AetherOS-64"
)

:: Get absolute path for AETHEROS_64_DIR
if not "%AETHEROS_64_DIR%"=="" (
    for %%i in ("%AETHEROS_64_DIR%") do set "AETHEROS_64_DIR=%%~fi"
)

if "%AETHEROS_64_DIR%"=="" (
    echo [ERROR] AetherOS-64 directory containing AetherOS.iso was not found!
    pause
    exit /b 1
)

set "ISO_PATH=%AETHEROS_64_DIR%\AetherOS.iso"
set "KEYS_ISO_PATH=%AETHEROS_64_DIR%\keys.iso"

echo Using ISO path: %ISO_PATH%

:: 3. Kill any background zombie/invisible processes to avoid locks
echo Cleaning up existing VirtualBox processes...
taskkill /F /IM VirtualBox.exe >nul 2>&1
taskkill /F /IM VirtualBoxVM.exe >nul 2>&1
timeout /t 1 /nobreak >nul

:: 4. Check if VM exists, and update configuration
echo Checking VM configuration...
"%VBOX_DIR%\VBoxManage.exe" list vms | findstr /i /c:"\"Aether OS\"" >nul
if %ERRORLEVEL% neq 0 (
    echo [WARNING] VM "Aether OS" does not exist yet. Running VM setup script...
    set "SETUP_PS1=%AETHEROS_64_DIR%\setup_vm.ps1"
    if exist "!SETUP_PS1!" (
        powershell -NoProfile -ExecutionPolicy Bypass -File "!SETUP_PS1!"
    ) else (
        echo [ERROR] setup_vm.ps1 not found at !SETUP_PS1!
        pause
        exit /b 1
    )
) else (
    echo VM "Aether OS" found. Dynamically updating storage mounts and shared folders...
    
    :: Update ISO mounts
    "%VBOX_DIR%\VBoxManage.exe" storageattach "Aether OS" --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium "%ISO_PATH%"
    if exist "%KEYS_ISO_PATH%" (
        "%VBOX_DIR%\VBoxManage.exe" storageattach "Aether OS" --storagectl "IDE" --port 1 --device 1 --type dvddrive --medium "%KEYS_ISO_PATH%"
    )
    
    :: Update Shared Folders
    "%VBOX_DIR%\VBoxManage.exe" sharedfolder remove "Aether OS" --name "AetherOS" >nul 2>&1
    "%VBOX_DIR%\VBoxManage.exe" sharedfolder add "Aether OS" --name "AetherOS" --hostpath "%AETHEROS_64_DIR%" --automount --auto-mount-point="/media/sf_AetherOS" --readonly
)

:: Set screen resolution hint so VirtualBox window boots cleanly
"%VBOX_DIR%\VBoxManage.exe" controlvm "Aether OS" setvideomodehint 1920 1080 32 >nul 2>&1

:: 5. Launch the VM frontend in the interactive session
echo Launching Aether OS in a visible window...
start "" "%VBOX_DIR%\VirtualBoxVM.exe" --startvm "Aether OS"

if %ERRORLEVEL% neq 0 (
    echo [WARNING] Direct frontend start failed. Attempting with VBoxManage...
    start "" "%VBOX_DIR%\VBoxManage.exe" startvm "Aether OS"
)

echo Aether OS VM has been launched successfully.
timeout /t 5 >nul
