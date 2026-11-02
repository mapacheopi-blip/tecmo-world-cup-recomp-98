param([Parameter(Mandatory=$true)][string]$Rom,[string]$GhidraHome=$env:GHIDRA_INSTALL_DIR,[int]$Jobs=4)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& python (Join-Path $PSScriptRoot 'prepare_inputs.py') --rom $Rom
if($LASTEXITCODE -ne 0){throw 'Unsupported ROM or input preparation failed.'}
& (Join-Path $PSScriptRoot 'export_aot.ps1') -GhidraHome $GhidraHome
& cmake -S (Join-Path $root 'native-port') -B (Join-Path $root 'native-port/build') -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
if($LASTEXITCODE -ne 0){throw 'CMake failed.'}
& cmake --build (Join-Path $root 'native-port/build') --target tecmo98-native-launcher-player --parallel $Jobs
if($LASTEXITCODE -ne 0){throw 'Native build failed.'}
& (Join-Path $PSScriptRoot 'package.ps1')
