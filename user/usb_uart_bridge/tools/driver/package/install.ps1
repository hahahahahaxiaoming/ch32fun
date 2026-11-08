param([switch]$TrustLocalTestCertificate)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
try {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Run USB_Bridge_Driver_Setup.exe as administrator.'
    }
    $catalog = Join-Path $PSScriptRoot 'usb_bridge.cat'
    $certificateFile = Join-Path $PSScriptRoot 'usb_bridge_test.cer'
    $expectedThumbprint = '7B25106B89374B135C89C10170A9B79CEB090F64'
    if (-not (Test-Path -LiteralPath $catalog)) { throw 'Missing usb_bridge.cat.' }
    $signature = Get-AuthenticodeSignature -LiteralPath $catalog
    if (-not $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $expectedThumbprint) {
        throw 'Catalog signer does not match this test package.'
    }
    if ($TrustLocalTestCertificate) {
        $cert = New-Object Security.Cryptography.X509Certificates.X509Certificate2($certificateFile)
        if ($cert.Thumbprint -ne $expectedThumbprint) { throw 'Unexpected test certificate.' }
        Import-Certificate -FilePath $certificateFile -CertStoreLocation Cert:/LocalMachine/Root | Out-Null
        Import-Certificate -FilePath $certificateFile -CertStoreLocation Cert:/LocalMachine/TrustedPublisher | Out-Null
    }
    $signature = Get-AuthenticodeSignature -LiteralPath $catalog
    if ($signature.Status -ne 'Valid') {
        throw 'Test certificate is not trusted. Use the driver setup and explicitly accept local test-certificate trust.'
    }
    & "$env:WINDIR\System32\pnputil.exe" /add-driver (Join-Path $PSScriptRoot 'usb_bridge.inf') /install
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3010) { throw "PnPUtil failed: $LASTEXITCODE" }
    Write-Host 'Package installed. Reconnect the device and verify USB UART / USB RS485 and COM port.'
    Write-Host 'If Windows rejected the package, do not change boot security automatically; keep the installation output for diagnosis.'
} catch {
    Write-Host ('INSTALL FAILED: ' + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
