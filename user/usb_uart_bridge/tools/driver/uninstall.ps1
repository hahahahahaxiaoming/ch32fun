param([switch]$WhatIf)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding($false)
$thumbprints=@('7B25106B89374B135C89C10170A9B79CEB090F64','0F02C45DB8FB05D5652680D837FED64D12018CAA')
try {
    if (-not $WhatIf) {
        $principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
        if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run the driver setup as administrator.' }
    }
    $matched=@()
    foreach($driver in Get-WindowsDriver -Online -All) {
        if($driver.ProviderName -ne 'USB Bridge' -or $driver.ClassName -ne 'Ports') {continue}
        if($driver.Driver -notmatch '^oem[0-9]+\.inf$') {continue}
        if([IO.Path]::GetFileName($driver.OriginalFileName) -ine 'usb_bridge.inf') {continue}
        if(-not (Test-Path -LiteralPath $driver.OriginalFileName)) {continue}
        $text=Get-Content -LiteralPath $driver.OriginalFileName -Raw
        if(-not $text.Contains('USB\VID_1A86&PID_0001') -or -not $text.Contains('USB\VID_1A86&PID_0002')) {continue}
        $cat=Join-Path ([IO.Path]::GetDirectoryName($driver.OriginalFileName)) 'usb_bridge.cat'
        if(-not (Test-Path -LiteralPath $cat)) {continue}
        $signature=Get-AuthenticodeSignature -LiteralPath $cat
        if(-not $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -notin $thumbprints) {continue}
        $matched+=$driver.Driver
    }
    foreach($publishedName in $matched) {
        if($WhatIf) { Write-Output "Would uninstall: $publishedName"; continue }
        & "$env:WINDIR\System32\pnputil.exe" /delete-driver $publishedName /uninstall
        if($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3010) {throw "Failed to uninstall $publishedName; code $LASTEXITCODE. Certificate trust retained."}
    }
    if(-not $matched.Count) {Write-Output 'No matching USB Bridge test driver package found.'}
    foreach($thumbprint in $thumbprints) {
      foreach($store in @('Root','TrustedPublisher')) {
        $path="Cert:/LocalMachine/$store/$thumbprint"
        if($WhatIf) {Write-Output "Would remove only this test certificate: $path"}
        elseif(Test-Path -LiteralPath $path) {Remove-Item -LiteralPath $path}
      }
    }
    Write-Output 'Completed. Windows usbser.sys and other drivers were not removed.'
} catch {
    Write-Host ('UNINSTALL FAILED: '+$_.Exception.Message) -ForegroundColor Red
    exit 1
}
