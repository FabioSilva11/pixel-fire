param([ValidateSet('arm64-v8a','armeabi-v7a')][string]$Abi='arm64-v8a', [string]$Serial='', [switch]$BuildOnly)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=Join-Path $repo 'output/world'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$sdk=Join-Path $env:LOCALAPPDATA 'Android/Sdk'
$target=if($Abi -eq 'armeabi-v7a'){'armv7a-linux-androideabi24'}else{'aarch64-linux-android24'}
$cc=Join-Path $sdk "ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/$target-clang++.cmd"
$adb=Join-Path $sdk 'platform-tools/adb.exe'
$device=@(); if($Serial){$device=@('-s',$Serial)}
$native=Join-Path $repo 'app/src/main/cpp'
$sources=@('tests/native/world_interactions_tests.cpp','app/src/main/cpp/world/world.cpp',
    'app/src/main/cpp/world/fluids.cpp','app/src/main/cpp/world/destruction.cpp') |
    ForEach-Object {Join-Path $repo $_}
Push-Location $build
try {
    $binary="world_tests-$Abi"
    & $cc -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wno-unused-parameter -DZZ_PERF_HOST "-I$native" @sources -static-libstdc++ -o $binary
    if($LASTEXITCODE -ne 0){throw 'Native world tests did not compile'}
    if($BuildOnly){return}
    & $adb @device push $binary /data/local/tmp/zz_world_tests
    if($LASTEXITCODE -ne 0){throw 'Test device unavailable'}
    & $adb @device shell chmod 755 /data/local/tmp/zz_world_tests
    & $adb @device shell /data/local/tmp/zz_world_tests
    if($LASTEXITCODE -ne 0){throw 'World tests failed'}
}finally{Pop-Location}
