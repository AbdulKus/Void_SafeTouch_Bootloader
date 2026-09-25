[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    cmake -S . -B build -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    cmake --build build --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
    Write-Host "Output: $PSScriptRoot\build\$Configuration"
} finally {
    Pop-Location
}
