param([switch]$FactoryImage)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$appPath = Join-Path $project 'build\CH570\usb_daplink.bin'
[byte[]]$appBytes = [IO.File]::ReadAllBytes($appPath)
if ($appBytes.Length -lt 24 -or $appBytes.Length -gt (0x3c000 - 0x1000)) {
    throw 'APP is outside the CH570 application region.'
}
$entry = [BitConverter]::ToUInt32($appBytes,0)
if (($entry -band 0x7f) -ne 0x6f -and ($entry -band 0xe003) -ne 0xa001) { throw 'Missing entry jump.' }
if ([BitConverter]::ToUInt32($appBytes,0x14) -ne 4093230505) { throw 'Missing vector tag at 0x1014.' }
function Assert-Contains([byte[]]$Pattern, [string]$Name) {
    for ($i = 0; $i -le $appBytes.Length - $Pattern.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $Pattern.Length; $j++) {
            if ($appBytes[$i+$j] -ne $Pattern[$j]) { $match = $false; break }
        }
        if ($match) { return }
    }
    throw "Missing $Name in APP."
}
Assert-Contains @(18,1,0,2,0,0,0,64,0x86,0x1a,3,0) 'USB identity'
Assert-Contains @(9,2,32,0,1,1,0,128,50,9,4,0,0,2,255,0,0,2,7,5,2,2,64,0,0,7,5,129,2,64,0,0) 'WinUSB-only configuration'
Assert-Contains ([Text.Encoding]::Unicode.GetBytes('DeviceInterfaceGUIDs')) 'Microsoft properties'
Assert-Contains ([Text.Encoding]::Unicode.GetBytes("{CDB3B5AD-293B-4663-AA36-1AAE46463776}`0`0")) 'CMSIS-DAP interface GUID'
Write-Host "PASS: $appPath ($($appBytes.Length) bytes), APP address 0x1000"
if ($FactoryImage) {
    $bootPath = Join-Path $project '..\usb_bootloader\build\CH570\usb_bootloader.bin'
    [byte[]]$bootBytes = [IO.File]::ReadAllBytes($bootPath)
    if ($bootBytes.Length -eq 0 -or $bootBytes.Length -gt 4096) { throw 'Invalid bootloader size.' }
    [byte[]]$combined = New-Object byte[] (4096 + $appBytes.Length)
    for ($i = 0; $i -lt 4096; $i++) { $combined[$i] = 255 }
    [Array]::Copy($bootBytes,0,$combined,0,$bootBytes.Length)
    [Array]::Copy($appBytes,0,$combined,4096,$appBytes.Length)
    $output = Join-Path $project 'build\CH570\usb_daplink_factory.bin'
    [IO.File]::WriteAllBytes($output,$combined)
    Write-Host "Factory image: $output (program at 0x0000)"
}
