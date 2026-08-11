# Build & run POC Terminal Tracker (Windows, Qt 6.11.1 + MinGW 13.10)
# Usage:
#   .\run.ps1            - run the app (already built)
#   .\run.ps1 -Build     - build and run
#   .\run.ps1 -Test      - build and run tests (ctest)
#   .\run.ps1 -CheckDb   - check DB connection & migrations (--check-db)
param(
    [switch]$Build,
    [switch]$Test,
    [switch]$CheckDb
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$cmake = "E:\Qt\Tools\CMake_64\bin\cmake.exe"
$qtBin = "E:\Qt\6.11.1\mingw_64\bin"
$mingwBin = "E:\Qt\Tools\mingw1310_64\bin"
$pgBin = "G:\Program Files\PostgreSQL\17\bin"
$qtPlugins = "E:\Qt\6.11.1\mingw_64\plugins"
$pgRoot = "G:\Program Files\PostgreSQL\17"

if (-not (Test-Path $cmake)) { throw "cmake not found: $cmake" }
if (-not (Test-Path "$qtBin\qmake.exe")) { throw "Qt not found: $qtBin" }
if (-not (Test-Path $pgBin)) { throw "PostgreSQL not found: $pgBin" }

$env:PATH = "$qtBin;$mingwBin;$pgBin;$env:PATH"
$env:QT_PLUGIN_PATH = $qtPlugins

$buildDir = "$root\build\dev-mingw"
$exe = "$buildDir\POC_Terminal_Tracker.exe"

if ($Build -or $Test -or -not (Test-Path $exe)) {
    Write-Host "==> Configure (dev-mingw)..." -ForegroundColor Cyan
    & $cmake --preset dev-mingw "-DPostgreSQL_ROOT=$pgRoot"
    if ($LASTEXITCODE -ne 0) { throw "Configure failed" }

    Write-Host "==> Build..." -ForegroundColor Cyan
    & $cmake --build --preset dev-mingw
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

# The app loads config.json from the build dir; put .env next to it
if (Test-Path "$root\config\.env") {
    Copy-Item "$root\config\.env" "$buildDir\config\.env" -Force
}

if ($Test) {
    Write-Host "==> Tests (ctest)..." -ForegroundColor Cyan
    $ctest = "$(Split-Path $cmake)\ctest.exe"
    & $ctest --preset dev-mingw --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
    exit $LASTEXITCODE
}

if ($CheckDb) {
    Write-Host "==> DB check (--check-db)..." -ForegroundColor Cyan
    & $exe --check-db
    exit $LASTEXITCODE
}

Write-Host "==> Launching app..." -ForegroundColor Cyan
& $exe
