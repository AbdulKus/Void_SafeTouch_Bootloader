[CmdletBinding()]
param(
    [string]$BuildDirectory = "$PSScriptRoot\build\Release"
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run install.ps1 from an elevated PowerShell window.'
}
if (-not [Environment]::Is64BitProcess) { throw 'Run the installer from 64-bit PowerShell.' }
$dll = Join-Path $BuildDirectory 'SafeTouchCredentialProvider.dll'
$setup = Join-Path $BuildDirectory 'SafeTouchSetup.exe'
if (-not (Test-Path -LiteralPath $dll) -or -not (Test-Path -LiteralPath $setup)) {
    throw "Build output was not found in $BuildDirectory"
}
$destination = Join-Path $env:ProgramFiles 'SafeTouch'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination $destination -Force
Copy-Item -LiteralPath $setup -Destination $destination -Force
$clsid = '{C77E1F56-30A7-4D92-84B7-4BCB6B3213A8}'
$comKey = "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$clsid"
$providerKey = "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$clsid"
New-Item -Path "$comKey\InprocServer32" -Force | Out-Null
Set-Item -Path $comKey -Value 'SafeTouch Credential Provider'
Set-Item -Path "$comKey\InprocServer32" -Value (Join-Path $destination 'SafeTouchCredentialProvider.dll')
New-ItemProperty -Path "$comKey\InprocServer32" -Name ThreadingModel -Value Apartment -PropertyType String -Force | Out-Null
New-Item -Path $providerKey -Force | Out-Null
Set-Item -Path $providerKey -Value 'SafeTouch'
Write-Host 'Credential Provider installed. The built-in password provider was not changed.'
Write-Host "Next: run elevated `"$destination\SafeTouchSetup.exe`" while SafeTouch is connected."
