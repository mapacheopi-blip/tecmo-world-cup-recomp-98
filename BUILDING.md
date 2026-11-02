# Building from source

To **play**, use the Windows release ZIP and your own ROM. These tools are for
development only. Never commit the ROM, prepared cartridge, IR or generated C++.

## Verified tools

- Windows x64, PowerShell and Python 3.
- .NET SDK 10.
- LLVM-MinGW with `clang++`, CMake and Ninja on PATH.
- Ghidra **12.1.4** and a compatible JDK (the development setup uses JDK 25).
  The specification/generator includes explicit SH-2 and 68000 fixes;
  revalidate translations when updating Ghidra.
- Your own supported game ROM.

```powershell
$env:GHIDRA_INSTALL_DIR = 'C:\Tools\ghidra_12.1.4_PUBLIC'
.\tools\build.ps1 -Rom 'C:\MyGames\twcup98.zip' -Jobs 4
```

The pipeline verifies five chips by hash, rebuilds `work/cart.bin`, extracts
the sound driver from that cartridge, exports Ghidra IR and generates static
C++. It then builds `tecmo98-native-launcher-player` and publishes the
self-contained launcher and dependencies to `dist/windows`.

The final package includes the BIOS and **only 16 KiB** of resident firmware,
verified by hash. It excludes the old full 1 MiB RAM capture, ROM, `work/` and
`DataNative/`. Run import/play checks in a separate copy to keep the package clean.

To repeat packaging after compilation:

```powershell
.\tools\package.ps1
```

Packaging stops if the destination contains `DataNative`, preventing an active
installation from accidentally becoming a public package. For an update,
choose a new clean destination with `-Destination`.

## Checks without a ROM

```powershell
cmake -S native-port -B native-port/build-check
cmake --build native-port/build-check --config Release --target tecmo98-launcher-input-check tecmo98-contact-mailbox-check tecmo98-presentation-clock-check
dotnet build launcher/Tecmo98.csproj -c Release
python tools/audit_publication.py
```

The launcher also provides `--self-test` and `--localization-check`. The latter
opens a temporary WPF test window, switches the actual English/Spanish selector,
checks persistence and controller navigation, and renders both languages at
1280×820 and 1024×720. Results are written under `verification/`; it uses a
controller fixture, not a physical device. `--ui-proof` additionally imports
your local ROM to check the Select ROM → Start transition.

These ROM-free checks do not run the game. Fidelity checks using original data
need your local files. Build and verification folders are ignored; public CI
does not receive private game data.

## Launcher localization

`launcher/Localization.cs` contains the English/Spanish presentation catalog.
English is the default; `Language` in local `settings.json` saves the choice.
Keep page IDs, graphics/filter values, input bindings and hardware identities
independent of translated labels. Changing language must not change the native
launch settings or rebind a control. The original game's language is unchanged.
