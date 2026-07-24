$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$Lcc = "C:\Dev_tools\gbdk\bin\lcc.exe"
$Output = Join-Path $BuildDir "gb_dub_siren.gb"

if (-not (Test-Path -LiteralPath $Lcc)) {
    throw "GBDK compiler not found: $Lcc"
}

if (-not (Test-Path -LiteralPath $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

& $Lcc -Wa-l -Wl-m -Wl-j -o $Output `
    (Join-Path $ProjectRoot "src\main.c")

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $Output)) {
    throw "ROM was not generated: $Output"
}

Write-Host "Generated ROM: $Output"

