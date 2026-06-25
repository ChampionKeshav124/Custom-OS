# AetherOS VirtualBox Automated VM Configuration Script
# Run this script every time you want to update your API key or reconfigure the VM.
# Just update api_key.txt and run this script - the VM will auto-read it on next boot.

$ErrorActionPreference = "Stop"

# 1. Locate VBoxManage
$VBoxManage = ""
$defaultPaths = @(
    "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe",
    "${env:ProgramFiles}\Oracle\VirtualBox\VBoxManage.exe",
    "${env:ProgramFiles(x86)}\Oracle\VirtualBox\VBoxManage.exe"
)

foreach ($path in $defaultPaths) {
    if (Test-Path $path) {
        $VBoxManage = $path
        break
    }
}

if (-not $VBoxManage) {
    $VBoxManage = Get-Command VBoxManage.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $VBoxManage) {
    Write-Error "VirtualBox does not appear to be installed, or VBoxManage.exe is not in the system PATH."
    exit 1
}

Write-Host "Found VBoxManage at: $VBoxManage"

$VMName = "Aether OS"
$ISOPath = Join-Path $PSScriptRoot "AetherOS.iso"
$ApiKeyPath = Join-Path $PSScriptRoot "api_key.txt"
$SharedFolderName = "AetherOS"

if (-not (Test-Path $ISOPath)) {
    Write-Warning "AetherOS.iso not found in the script directory ($PSScriptRoot). Please ensure the ISO has been built before running this script."
}

# 2. Check if VM already exists
$vmExists = $false
$vms = & $VBoxManage list vms
foreach ($line in $vms) {
    if ($line -match """$VMName""") {
        $vmExists = $true
        break
    }
}

if ($vmExists) {
    Write-Host "VM '$VMName' already exists. Re-configuring and applying correct settings..."
    & $VBoxManage modifyvm $VMName --ostype Ubuntu_64 --memory 2048 --vram 128 --cpus 2 --longmode on --pae on --acpi on --ioapic on --graphicscontroller VMSVGA --accelerate3d off --boot1 dvd --boot2 disk --nic1 nat --clipboard bidirectional --draganddrop bidirectional --defaultfrontend gui
    Write-Host "VM '$VMName' has been updated with the correct settings."
} else {
    Write-Host "Creating a new VM '$VMName'..."
    & $VBoxManage createvm --name $VMName --ostype Ubuntu_64 --register
    & $VBoxManage modifyvm $VMName --memory 2048 --vram 128 --cpus 2 --longmode on --pae on --acpi on --ioapic on --graphicscontroller VMSVGA --accelerate3d off --boot1 dvd --boot2 disk --nic1 nat --clipboard bidirectional --draganddrop bidirectional --defaultfrontend gui

    # Add Storage Controller
    & $VBoxManage storagectl $VMName --name "IDE" --add ide --controller PIIX4 --bootable on

    # Create a Virtual Hard Disk (20 GB)
    $vmDir = & $VBoxManage list systemproperties | Select-String "Default machine folder:"
    $vmDir = $vmDir -replace "Default machine folder:\s+",""
    $vdiDir = Join-Path $vmDir $VMName
    $vdiPath = Join-Path $vdiDir "$VMName.vdi"

    if (-not (Test-Path $vdiDir)) {
        New-Item -ItemType Directory -Path $vdiDir -Force | Out-Null
    }

    Write-Host "Creating virtual disk at: $vdiPath..."
    & $VBoxManage createmedium disk --filename $vdiPath --size 20480 --format VDI
    & $VBoxManage storageattach $VMName --storagectl "IDE" --port 0 --device 0 --type hdd --medium $vdiPath

    Write-Host "VM '$VMName' created successfully."
}

# 3. Mount the OS ISO
if (Test-Path $ISOPath) {
    Write-Host "Mounting $ISOPath to '$VMName'..."
    & $VBoxManage storageattach $VMName --storagectl "IDE" --port 1 --device 0 --type dvddrive --medium $ISOPath
    Write-Host "OS ISO mounted successfully."
}

# 4. Convert api_key.txt into a Virtual CD-ROM (keys.iso) so AetherOS can read it natively
$KeysISOPath = Join-Path $PSScriptRoot "keys.iso"
$ApiKeyPath = Join-Path $PSScriptRoot "api_key.txt"

if (Test-Path $ApiKeyPath) {
    Write-Host "Packaging api_key.txt into a Virtual CD-ROM (keys.iso)..."
    
    # Convert Windows path to WSL path
    $drive = $PSScriptRoot.Substring(0,1).ToLower()
    $linuxPath = $PSScriptRoot.Substring(3).Replace("\", "/")
    $wslDir = "/mnt/$drive/$linuxPath"
    
    $wslCmd = "mkdir -p /tmp/keys && cp '$wslDir/api_key.txt' /tmp/keys/ && xorriso -as mkisofs -o '$wslDir/keys.iso' -V 'KEYS' -J -R /tmp/keys/ -quiet && rm -rf /tmp/keys"
    
    wsl -u root -- bash -c $wslCmd
    
    if (Test-Path $KeysISOPath) {
        Write-Host "Keys ISO generated."
    }
}

if (Test-Path $KeysISOPath) {
    Write-Host "Mounting $KeysISOPath to '$VMName'..."
    & $VBoxManage storageattach $VMName --storagectl "IDE" --port 1 --device 1 --type dvddrive --medium $KeysISOPath
    Write-Host "API Keys CD-ROM mounted successfully."
}

# 5. Also inject key via Guest Property as a fallback (works without mounting)
if (Test-Path $ApiKeyPath) {
    $ApiKey = Get-Content $ApiKeyPath -Raw
    if ($ApiKey) {
        $ApiKey = $ApiKey.Trim()
        if ($ApiKey -and ($ApiKey -notmatch "ENTER YOUR GEMINI API KEY HERE")) {
            Write-Host "Also injecting API Key via VM property as fallback..."
            & $VBoxManage guestproperty set $VMName "/AetherOS/GeminiAPIKey" $ApiKey
            Write-Host "API Key injected via VM property."
        }
    }
}

Write-Host ""
Write-Host "=========================================="
Write-Host " All done! How to update your API key:"
Write-Host "  1. Edit api_key.txt in this folder"
Write-Host "  2. Run this script again"
Write-Host "  3. Restart the Virtual Machine"
Write-Host "=========================================="
Write-Host ""
Write-Host "You can now start the '$VMName' Virtual Machine in VirtualBox."
