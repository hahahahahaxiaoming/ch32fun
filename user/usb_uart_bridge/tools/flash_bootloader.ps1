param(
    [ValidateRange(0, 3600)][int]$TimeoutSeconds = 120,
    [ValidateSet('uart', 'rs485')][string]$Mode,
    [switch]$DetectOnly,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

# CH570 factory USB ISP settings shared by the UART and RS485 download modes.
# Use $true or $false for switches.
$CodeProtect        = $true    # Keep code/data protection enabled as in the vendor USB profile.
$ResetPinEnabled    = $false   # Enable external manual reset.
$ResetPin           = 'PA8'    # PA8 or PA7. This bridge already uses PA7.
$IndependentWatchdog = $false  # Start IWDG in hardware. The APP must feed it if enabled.
$SerialNoButton     = $false   # Allow future UART ISP entry without the BOOT button.
$RunAfterDownload   = $true    # Start the installed firmware after programming.
$EraseCodeFlash     = $true    # Enable only for Bootloader + APP factory downloads.
$ProtectBytes       = 0        # 0 disables address write protection, 0x1000 protects Bootloader.
$DownloadBaud       = 1000000  # Vendor UART field. This script uses USB.

if ($ResetPin -notin @('PA7', 'PA8')) { throw 'ResetPin must be PA7 or PA8.' }
if ($ResetPinEnabled -and $ResetPin -eq 'PA7') {
    throw 'PA7 is used by the bridge and cannot be enabled as external reset.'
}
if ($ProtectBytes -notin @(0, 0x1000)) {
    throw 'ProtectBytes must be 0 or 0x1000 so the APP remains writable.'
}
if ($EraseCodeFlash -and !$Mode -and !$DetectOnly -and !$DryRun) {
    throw 'Full CodeFlash erase requires a Bootloader + APP factory download.'
}

$profileLine = 3   # McuSer in the vendor chip list.
$romLine = 0x13    # Family byte returned by the CH570 ROM ISP.
$chipType = 0x70
$image = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\usb_bootloader\build\CH570\usb_bootloader.bin'))
$appAddress = 0x1000
if ($Mode) {
    $appImage = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\build\CH570\$Mode\usb_$Mode.bin"))
}
$ispDirectory = Join-Path $PSScriptRoot 'WCHISPTool_CH57x-59x'
$dll = Join-Path $ispDirectory 'WCH55xISPDLL.dll'
if ($DryRun) {
    Write-Host "USB ISP DLL: $dll"
    Write-Host "CH570 (profile line $profileLine, ROM line $romLine, type $chipType): $image at 0x0000"
    if ($Mode) { Write-Host "CH570 ${Mode}: $appImage at 0x1000" }
    Write-Host "CodeProtect=$CodeProtect ResetPinEnabled=$ResetPinEnabled ResetPin=$ResetPin IndependentWatchdog=$IndependentWatchdog"
    Write-Host "SerialNoButton=$SerialNoButton RunAfterDownload=$RunAfterDownload EraseCodeFlash=$EraseCodeFlash ProtectBytes=$ProtectBytes DownloadBaud=$DownloadBaud"
    exit 0
}
if (!(Test-Path -LiteralPath $dll -PathType Leaf)) {
    throw "Missing WCH USB ISP DLL: $dll"
}

if ([Environment]::Is64BitProcess) {
    $x86PowerShell = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    if (!(Test-Path -LiteralPath $x86PowerShell -PathType Leaf)) {
        throw '32-bit Windows PowerShell is required by WCH55xISPDLL.dll.'
    }
    $childArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath,
                   '-TimeoutSeconds', $TimeoutSeconds)
    if ($Mode) { $childArgs += @('-Mode', $Mode) }
    if ($DetectOnly) { $childArgs += '-DetectOnly' }
    & $x86PowerShell @childArgs
    exit $LASTEXITCODE
}

if (!$DetectOnly) {
    if (!(Test-Path -LiteralPath $image -PathType Leaf)) {
        throw "Missing $image. Run 'make -C ../usb_bootloader CH570' in usb_uart_bridge."
    }
    $code = [IO.File]::ReadAllBytes($image)
    if ($code.Length -lt 4 -or $code.Length -gt $appAddress -or
        (([BitConverter]::ToUInt32($code, 0) -band 0x7f) -ne 0x6f)) {
        throw "Invalid bootloader image: $image"
    }

    if ($Mode) {
        if (!(Test-Path -LiteralPath $appImage -PathType Leaf)) {
            throw "Missing $appImage. Run 'make $Mode' in usb_uart_bridge."
        }
        $app = [IO.File]::ReadAllBytes($appImage)
        if ($app.Length -lt 24 -or $app.Length -gt 236KB -or
            [BitConverter]::ToUInt32($app, 0x14) -ne [Convert]::ToUInt32('F3F9BDA9', 16)) {
            throw "Invalid CH570 APP image at 0x1000: $appImage"
        }
        $entry = [BitConverter]::ToUInt32($app, 0)
        if (($entry -band 0x7f) -ne 0x6f -and ($entry -band 0xe003) -ne 0xa001) {
            throw "Invalid APP entry instruction: $appImage"
        }
        $deviceDescriptors = @(
            '12-01-00-02-02-02-00-40-86-1A-'
            '12-01-00-02-EF-02-01-40-86-1A-'
        )
        $appHex = [BitConverter]::ToString($app)
        $expectedPid = if ($Mode -eq 'uart') { '01-00' } else { '02-00' }
        $otherPid = if ($Mode -eq 'uart') { '02-00' } else { '01-00' }
        $matchesMode = $false
        $matchesOtherMode = $false
        foreach ($descriptor in $deviceDescriptors) {
            $matchesMode = $matchesMode -or $appHex.Contains($descriptor + $expectedPid)
            $matchesOtherMode = $matchesOtherMode -or $appHex.Contains($descriptor + $otherPid)
        }
        if (!$matchesMode -or $matchesOtherMode) {
            throw "APP USB identity does not match ${Mode}: $appImage"
        }

        $combined = New-Object byte[] ($appAddress + $app.Length)
        [Array]::Copy($code, $combined, $code.Length)
        for ($offset = $code.Length; $offset -lt $appAddress; $offset++) {
            $combined[$offset] = 0xff
        }
        [Array]::Copy($app, 0, $combined, $appAddress, $app.Length)
        $code = $combined
    }
}

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class BridgeWchIsp {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr LoadLibrary(string path);
    [DllImport("WCH55xISPDLL.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int WCH55x_SetIspOption([In] byte[] options);
    [DllImport("WCH55xISPDLL.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern byte WCH55x_EnumDevices([Out] byte[] devices, byte capacity,
        ref byte line, ref byte type, ref uint extra);
    [DllImport("WCH55xISPDLL.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int WCH55x_GetIspDeviceInfor(int index, IntPtr record);
    [DllImport("WCH55xISPDLL.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int WCH55x_FlashProgramB(int index, [In] byte[] code, uint codeLength,
        int codeSelected, IntPtr dataFlash, uint dataFlashLength, IntPtr reserved);
    [DllImport("WCH55xISPDLL.dll", CallingConvention = CallingConvention.StdCall)]
    public static extern int WCH55x_CloseDevice(int index);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void LogCallback(int level, IntPtr message);
    private static readonly LogCallback Log = (level, message) => {
        string line = Marshal.PtrToStringUni(message);
        if (!String.IsNullOrEmpty(line)) Console.WriteLine("[WCH/{0}] {1}", level, line);
    };
    public static int LogPointer() { return Marshal.GetFunctionPointerForDelegate(Log).ToInt32(); }
}
'@

if ([BridgeWchIsp]::LoadLibrary($dll) -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "Cannot load $dll (Windows error $errorCode)."
}

# WCH v3.8 USB profile, adapted from CH592F_DEMO/TOOLS/flash_usb.ps1.
# The CH570 profile has no separate DataFlash image or erase setting.
$options = New-Object byte[] 960
$options[4] = $chipType
$options[5] = $profileLine
$options[6] = [byte]($ResetPin -eq 'PA7')
$options[14] = [byte]$ResetPinEnabled
$options[22] = [byte]$RunAfterDownload
$options[42] = [byte]($ProtectBytes / 4KB)
$options[48] = [byte]$CodeProtect
$options[844] = [byte]$SerialNoButton
$options[848] = 1 # Factory BOOT selection.
$options[857] = [byte]$EraseCodeFlash
$options[906] = 1 # USB profile.
$options[930] = [byte]$IndependentWatchdog
[Array]::Copy([BitConverter]::GetBytes([BridgeWchIsp]::LogPointer()), 0, $options, 889, 4)
[Array]::Copy([BitConverter]::GetBytes([uint32]$DownloadBaud), 0, $options, 955, 4)

$capacity = 32
$recordSize = 71
if ([BridgeWchIsp]::WCH55x_SetIspOption($options) -eq 0) {
    throw 'WCH USB ISP option initialization failed for CH570.'
}
$devices = New-Object byte[] ($capacity * $recordSize)
$timer = [Diagnostics.Stopwatch]::StartNew()
Write-Host "Waiting for CH570 via USB ISP (up to ${TimeoutSeconds}s). Connect only the target board in factory ISP mode."
do {
    $line = [byte]0
    $type = [byte]0
    $extra = [uint32]0
    $count = [BridgeWchIsp]::WCH55x_EnumDevices($devices, $capacity, [ref]$line, [ref]$type, [ref]$extra)
    if ($count -gt 0) { break }
    if ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) { Start-Sleep -Milliseconds 250 }
} while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds)

if ($count -eq 0) {
    throw 'No CH570 factory USB ISP device found. Hold the factory BOOT key while powering the board.'
}
try {
    if ($count -ne 1) {
        throw 'Multiple USB ISP devices found. Disconnect all boards except the target.'
    }
    if ($line -ne $romLine -or $type -ne $chipType) {
        throw "USB ISP did not confirm CH570 (line $line, type $type; expected $romLine, $chipType)."
    }
    $handle = [Runtime.InteropServices.GCHandle]::Alloc($devices, [Runtime.InteropServices.GCHandleType]::Pinned)
    try {
        if ([BridgeWchIsp]::WCH55x_GetIspDeviceInfor(0, $handle.AddrOfPinnedObject()) -eq 0) {
            throw 'Could not read CH570 USB ISP device information.'
        }
    } finally {
        $handle.Free()
    }
    Write-Host 'Detected CH570 in factory USB ISP.'
    if ($DetectOnly) { exit 0 }

    if ($EraseCodeFlash) { Write-Host 'Full CodeFlash erase is enabled.' }
    if ($Mode) {
        Write-Host "Programming Bootloader at 0x0000 and $Mode APP at 0x1000 ($($code.Length) bytes)..."
    } else {
        Write-Host "Programming $image at 0x0000 ($($code.Length) bytes)..."
    }
    if ([BridgeWchIsp]::WCH55x_FlashProgramB(0, $code, $code.Length, 1,
            [IntPtr]::Zero, 0, [IntPtr]::Zero) -eq 0) {
        throw 'WCH USB ISP reported programming failure.'
    }
    Write-Host 'USB ISP programming completed.'
} finally {
    for ($index = 0; $index -lt $count; $index++) {
        [void][BridgeWchIsp]::WCH55x_CloseDevice($index)
    }
}
