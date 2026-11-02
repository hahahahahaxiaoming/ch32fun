$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    New-Item -ItemType Directory -Force dist | Out-Null
    & $compiler /nologo /target:winexe /platform:x64 /optimize+ /win32manifest:src\app.manifest /out:dist\USB_Bridge_Setup.exe /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.ServiceProcess.dll src\Program.cs
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
    & $compiler /nologo /target:exe /platform:x64 /out:dist\USB_Bridge_Tests.exe /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.ServiceProcess.dll src\Program.cs
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & ./dist\USB_Bridge_Tests.exe --selftest
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
