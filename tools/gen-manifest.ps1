# tools/gen-manifest.ps1
# Emits hac.manifest + hac.manifest.sig into the given output directory.
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $OutDir,      # e.g. x64\Release
    [Parameter(Mandatory)] [string] $GameDir,     # e.g. C:\Games\CS\, contains cstrike.exe
    [Parameter(Mandatory)] [string] $PrivateKey,  # path to hac-<env>.private.key
    [string] $GameId = "cstrike-1.6",
    [string] $Endpoint = "https://ac-report.example.com/v1/events",
    [string] $CertPinSha256 = ""
)

$ErrorActionPreference = "Stop"

function Sha256Hex($path) {
    (Get-FileHash -Algorithm SHA256 $path).Hash.ToLowerInvariant()
}

$root = Resolve-Path $OutDir
$modules = @(
    @{ path = "HerculesAC\HerculesAC.aes"; file = Join-Path $root "HerculesAC\HerculesAC.aes" }
    @{ path = "GameMon.aes";               file = Join-Path $root "GameMon.aes" }
    @{ path = "GameMon64.aes";             file = Join-Path $root "GameMon64.aes" }
    @{ path = "GameProtect.dll";           file = Join-Path $root "GameProtect.dll" }
    @{ path = "GameProtect64.dll";         file = Join-Path $root "GameProtect64.dll" }
)

$manifest = [ordered]@{
    version   = 2
    issued_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    game      = [ordered]@{
        id           = $GameId
        starter      = "cstrike.exe"
        client       = "cstrike.exe"
        monitor_x86  = "GameMon.aes"
        monitor_x64  = "GameMon64.aes"
    }
    modules   = @()
    whitelist_sha256 = @()
    reporting = [ordered]@{
        endpoint               = $Endpoint
        server_cert_pin_sha256 = $CertPinSha256
    }
}

foreach ($m in $modules) {
    if (-not (Test-Path $m.file)) { throw "missing module: $($m.file)" }
    $entry = [ordered]@{
        path   = $m.path
        sha256 = Sha256Hex $m.file
        size   = (Get-Item $m.file).Length
    }
    $manifest.modules += $entry
    # Every module also whitelists itself for GameProtect injection self-exclusion
    $manifest.whitelist_sha256 += $entry.sha256
}

$manifestPath = Join-Path $root "hac.manifest"
$json = $manifest | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText($manifestPath, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "wrote $manifestPath"

$signer = Join-Path $PSScriptRoot "..\bin\x64\Release\sign-manifest.exe" | Resolve-Path
& $signer --sign $manifestPath --key $PrivateKey
if ($LASTEXITCODE -ne 0) { throw "sign-manifest failed" }
Write-Host "wrote $manifestPath.sig"
