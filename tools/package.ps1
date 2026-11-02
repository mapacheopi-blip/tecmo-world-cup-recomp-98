param([string]$Destination=(Join-Path $PSScriptRoot '../dist/windows'))
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if(Test-Path (Join-Path $Destination 'DataNative')){throw 'Destination contains user data. Choose a clean package directory.'}
& dotnet publish (Join-Path $root 'launcher/Tecmo98.csproj') -c Release -r win-x64 --self-contained true -o $Destination --nologo
if($LASTEXITCODE -ne 0){throw 'Launcher publication failed.'}
& dotnet publish (Join-Path $root 'launcher/Native/InputBridge/InputBridge.csproj') -c Release -r win-x64 --self-contained true -o (Join-Path $Destination 'InputBridge') --nologo
if($LASTEXITCODE -ne 0){throw 'Input bridge publication failed.'}
$player=Join-Path $Destination 'Player';$docs=Join-Path $Destination 'Documentos'
New-Item -ItemType Directory -Force -Path $player,$docs | Out-Null
Copy-Item (Join-Path $root 'native-port/build/player-launcher/tecmo98-native-player.exe') $player
Copy-Item (Join-Path $root 'launcher/Native/SDL3.dll') $player
Copy-Item (Join-Path $root 'firmware/bios-jp1.bin') $docs
Copy-Item (Join-Path $root 'firmware/resident-jp1.bin') (Join-Path $docs 'resident-profile.bin')
Copy-Item (Join-Path $root 'LICENSE') $Destination
Copy-Item (Join-Path $root 'NOTICE.md') $Destination
Copy-Item (Join-Path $root 'README.md') (Join-Path $Destination 'LEEME.md')
Copy-Item (Join-Path $root 'firmware/NOTICE.md') (Join-Path $docs 'NOTICE.md')
$licenses=Join-Path $Destination 'licenses'
New-Item -ItemType Directory -Force -Path $licenses | Out-Null
Copy-Item (Join-Path $root 'native-port/third-party') $licenses -Recurse
Copy-Item (Join-Path $root 'launcher/Native/SDL-LICENSE.txt') $licenses
Write-Output "Clean Windows package: $Destination"
