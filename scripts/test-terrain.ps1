param([ValidateSet('arm64-v8a','armeabi-v7a')][string]$Abi = 'arm64-v8a', [string]$Serial = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repo 'output/terrain'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$sdk = Join-Path $env:LOCALAPPDATA 'Android/Sdk'
$target = if ($Abi -eq 'armeabi-v7a') { 'armv7a-linux-androideabi24' } else { 'aarch64-linux-android24' }
$compiler = Join-Path $sdk "ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/$target-clang++.cmd"
$adb = Join-Path $sdk 'platform-tools/adb.exe'
$device = @()
if ($Serial) { $device = @('-s', $Serial) }
$native = Join-Path $repo 'app/src/main/cpp'
$sources = @('tests/native/terrain_tests.cpp', 'app/src/main/cpp/world/island.cpp',
    'app/src/main/cpp/world/structures.cpp', 'app/src/main/cpp/world/buildings.cpp',
    'app/src/main/cpp/world/world.cpp', 'app/src/main/cpp/world/horizonte_adapter.cpp',
    'app/src/main/cpp/world/terrain.cpp') | ForEach-Object { Join-Path $repo $_ }
$sources += Get-ChildItem (Join-Path $native 'world/horizonte') -Filter '*.cpp' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
Push-Location $build
try {
    & $compiler -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wno-unused-parameter -DZZ_PERF_HOST "-I$native" @sources -static-libstdc++ -o terrain_tests
    if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed' }
    & $adb @device push terrain_tests /data/local/tmp/zz_terrain_tests
    if ($LASTEXITCODE -ne 0) { throw 'Test device unavailable' }
    & $adb @device shell chmod 755 /data/local/tmp/zz_terrain_tests
    & $adb @device shell /data/local/tmp/zz_terrain_tests
    if ($LASTEXITCODE -ne 0) { throw 'Terrain tests failed' }
} finally { Pop-Location }
