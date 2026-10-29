param([ValidateSet('arm64-v8a','armeabi-v7a')][string]$Abi='arm64-v8a', [string]$Serial='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$build=Join-Path $repo 'output/character'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$sdk=Join-Path $env:LOCALAPPDATA 'Android/Sdk'
$target=if($Abi -eq 'armeabi-v7a'){'armv7a-linux-androideabi24'}else{'aarch64-linux-android24'}
$compiler=Join-Path $sdk "ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/$target-clang++.cmd"
$adb=Join-Path $sdk 'platform-tools/adb.exe'
$device=@(); if($Serial){$device=@('-s',$Serial)}
$native=Join-Path $repo 'app/src/main/cpp'
$binary=Join-Path $build "character_tests-$Abi"
& $compiler -std=c++17 -O0 -Wall -Wextra "-I$native" (Join-Path $repo 'tests/native/character_animation_tests.cpp') (Join-Path $native 'render/character.cpp') -static-libstdc++ -o $binary
if($LASTEXITCODE -ne 0){throw 'Character animation tests did not compile'}
& $adb @device push $binary /data/local/tmp/zz_character_tests
if($LASTEXITCODE -ne 0){throw 'Test device unavailable'}
& $adb @device shell chmod 755 /data/local/tmp/zz_character_tests
if($LASTEXITCODE -ne 0){throw 'Could not prepare test executable'}
& $adb @device shell /data/local/tmp/zz_character_tests
if($LASTEXITCODE -ne 0){throw 'Character animation tests failed'}
