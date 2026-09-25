[CmdletBinding()]
param([switch]$PurgeCredentials)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run uninstall.ps1 from an elevated PowerShell window.'
}
$clsid = '{C77E1F56-30A7-4D92-84B7-4BCB6B3213A8}'
$keys = @(
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$clsid",
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\$clsid"
)
foreach ($key in $keys) { if (Test-Path -LiteralPath $key) { Remove-Item -LiteralPath $key -Recurse -Force } }
$programFilesRoot = [IO.Path]::GetFullPath($env:ProgramFiles).TrimEnd('\')
$destination = [IO.Path]::GetFullPath((Join-Path $programFilesRoot 'SafeVoid'))
if (-not $destination.StartsWith("$programFilesRoot\", [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove unexpected path: $destination"
}
if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Recurse -Force }
if ($PurgeCredentials) {
    $credentials = Join-Path $env:ProgramData 'SafeVoid\credentials.dat'
    if (Test-Path -LiteralPath $credentials) { Remove-Item -LiteralPath $credentials -Force }
}
Write-Host 'SafeVoid Credential Provider removed. Sign out or reboot to unload an existing DLL instance.'
