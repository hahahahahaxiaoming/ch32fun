$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    $packageDirectory=Join-Path $PSScriptRoot 'output'
    New-Item -ItemType Directory -Force $packageDirectory | Out-Null
    $resourceDirectory=Join-Path $PSScriptRoot 'package'
    $compiler=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    $resources=@('usb_bridge.inf','usb_bridge.cat','usb_bridge_test.cer','install.ps1','uninstall.ps1') | ForEach-Object {
        $file=Join-Path $resourceDirectory $_
        if(!(Test-Path -LiteralPath $file -PathType Leaf)){throw "Missing package file: $file"}
        "/resource:$file,$_"
    }
    & $compiler /nologo /target:winexe /platform:anycpu /optimize+ /win32manifest:src\setup.manifest "/out:$(Join-Path $packageDirectory USB_Bridge_Driver_Setup.exe)" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll @resources src\Setup.cs
    if($LASTEXITCODE -ne 0){throw 'Installer build failed'}
    $testDirectory=Join-Path $PSScriptRoot '..\..\build\driver-tests'
    New-Item -ItemType Directory -Force $testDirectory | Out-Null
    $testExe=Join-Path $testDirectory 'setup_test.exe'
    & $compiler /nologo /target:exe /platform:anycpu "/out:$testExe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll @resources src\Setup.cs
    if($LASTEXITCODE -ne 0){throw 'Installer test build failed'}
    & $testExe --selftest
    if($LASTEXITCODE -ne 0){throw 'Installer selftest failed'}
} finally {Pop-Location}
