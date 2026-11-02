param([string]$GhidraHome=$env:GHIDRA_INSTALL_DIR,[switch]$ExportOnly)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(!$GhidraHome){throw 'Set GHIDRA_INSTALL_DIR or pass -GhidraHome (Ghidra 12.1.4).'}
$headless=Join-Path $GhidraHome 'support/analyzeHeadless.bat'
$projectDir=Join-Path $root 'work/ghidra'
New-Item -ItemType Directory -Force -Path $projectDir | Out-Null
$name='tecmo-build-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$seeds=(Get-Content (Join-Path $root 'native-port/aot-entries.json') -Raw | ConvertFrom-Json).entries
$mainLog=Join-Path $root 'work/export-main.log'
& $headless $projectDir $name -import (Join-Path $root 'work/main-expanded.bin') -loader BinaryLoader -loader-baseAddr 0x06004000 -processor 'SuperH:BE:32:SH-2' -noanalysis -scriptPath (Join-Path $root 'scripts') -postScript ExportNativeBlocks.java (Join-Path $root 'work/native-blocks.jsonl') (Join-Path $root 'work/boot-epr.bin') (Join-Path $root 'work/bios-jp1-be.bin') (Join-Path $root 'firmware/resident-jp1.bin') @seeds -log $mainLog
if($LASTEXITCODE -ne 0 -or !(Select-String -LiteralPath $mainLog -Pattern NATIVE_BLOCK_EXPORT_COMPLETE -Quiet)){throw "Main export failed; inspect $mainLog"}
$soundLog=Join-Path $root 'work/export-sound.log'
& $headless $projectDir ($name+'-sound') -import (Join-Path $root 'work/sound-driver.bin') -loader BinaryLoader -loader-baseAddr 0 -processor '68000:BE:32:default' -noanalysis -scriptPath (Join-Path $root 'scripts') -postScript ExportSoundBlocks.java (Join-Path $root 'work/sound-blocks.jsonl') (Join-Path $root 'work/sound-driver.bin') -log $soundLog
if($LASTEXITCODE -ne 0 -or !(Select-String -LiteralPath $soundLog -Pattern NATIVE_SOUND_EXPORT_COMPLETE -Quiet)){throw "Sound export failed; inspect $soundLog"}
if($ExportOnly){return}
& python (Join-Path $root 'native-port/generate_aot.py')
if($LASTEXITCODE -ne 0){throw 'Main C++ generation failed.'}
& python (Join-Path $root 'native-port/generate_sound_aot.py')
if($LASTEXITCODE -ne 0){throw 'Sound C++ generation failed.'}
