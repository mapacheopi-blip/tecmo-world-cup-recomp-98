# TECMO WORLD CUP RECOMP 98

An experimental native **TECMO World Cup '98 / World Soccer '98** port for
Windows x64, using static recompilation and native platform services.
The game runs C++ compiled for PC. The launcher does not start MAME or a CPU
interpreter.

**The download does not include the game ROM. Bring your own compatible copy
of `twcup98.zip`.** Prepared game data is generated locally on your computer.
BIOS and resident firmware are supplied separately and retain their original
rights: see [NOTICE.md](NOTICE.md).

## Download and play

1. Download the **Windows x64 ZIP** from [Releases](https://github.com/mapacheopi-blip/tecmo-world-cup-recomp-98/releases/latest).
2. Extract the **entire** archive to a writable folder.
3. Open `TecmoWorldCupRecomp98.exe`.
4. Click **Select ROM** and choose your `twcup98.zip`.
5. Once verification finishes, the same button becomes **Start**.

The Windows package includes its runtime. You do not need to compile anything
or install Ghidra, MAME or the .NET SDK to play. GitHub's **Source code** archives
are for developers; use the Windows x64 release asset for a ready-to-play build.

### English and Spanish

Use the **Language / Idioma** selector at the **top right** of the launcher to
switch between **English** and **Español** at any time. Menus, help text,
controller settings and launcher messages change immediately. Your choice is
saved automatically for the next launch. New installations default to English.
This changes the launcher language; the original game's text is unchanged.

## Requirements

- Windows x64 supported by .NET 10; its runtime is included in the package.
- A GPU/driver supporting Direct3D 11 and Windows 10+ DXGI flip presentation.
- Your own copy of the **JUET 980410 V1.000** game revision, verified by content.

Local testing used an RTX 5060 Ti and DualSense. This does not certify every
computer or controller. No unmeasured minimum CPU specification is claimed.

## Features

- Main menu: Select ROM / Start, Controls, Settings and Exit.
- English / Spanish launcher, with a persistent language selector at the top.
- Sharp, Smooth and **Pixelart 2x** (GPU Scale2x), Direct3D 11 and VSync.
- Windowed or borderless fullscreen; original 4:3 or stretched aspect ratio.
- SDL3 and an alternative DirectInput backend; button/key rebinding, deadzone,
  per-device profiles and an input monitor.
- Game volume from 0 to 100% and mute, independent of Windows volume.
- Game/audio execution separated from the window and input-file reader.
- Local preferences and prepared ROM data in `DataNative/`; per-session logs.

Original game speed is preserved (approximately 59.8 frames/s). Pixelart 2x
changes scaling; it does not add HD geometry or increase internal resolution.
At a fixed 144 Hz refresh rate, frames may alternate between two and three
display refreshes. VRR, frame interpolation, RTX Remix and mods are not
implemented.

## Default controls

| Action | Player 1 keyboard | Default DualSense |
|---|---|---|
| Movement | Arrow keys | D-pad / left stick |
| Buttons A/B/C/D | Z / X / C / V | × / ○ / □ / △ |
| Start | 1 | Options |
| Insert coin | 5 | Create / Share |
| Return to launcher | Esc | Close the game window |

Bindings are configurable. Input expires after 250 ms; losing focus releases
inputs unless background input is enabled. Up to four player profiles are
available, subject to the original board's wiring. A complete multiplayer
match with every supported device has not been certified.

## Supported ROM

Older filenames inside the ZIP also work: the importer checks size and SHA-1.

| Chip | Bytes | SHA-1 |
|---|---:|---|
| epr20819.24 | 1048576 | f66cc955181720661a0334fe67fa5750ddf9758b |
| mpr20821.12 | 4194304 | 5fcaf4257f3639cb3aa407d2936f616499a09d97 |
| mpr20822.13 | 4194304 | d5689ac8aad63509febe9aa4077351be09b2d8d4 |
| mpr20823.14 | 4194304 | c387d03ba27580c62ac0bf780915fdf41552df6f |
| mpr20824.15 | 4194304 | 310961a5f114fea8938a3f514dffd5231e910a5a |

The original ZIP is read without modification. After import, prepared local
data lets you play without keeping that ZIP at its previous path. These files
still contain game data and must not be uploaded to GitHub.

## Status and limitations

The maintainer accepted the playable native build after hands-on testing.
Automated checks cover pixels, input transport, audio and presentation stalls;
they do not prove complete original-hardware fidelity, every effect or a full
tournament. Startup uses prepared resident firmware rather than a complete
BIOS cold boot.

Direct3D 11 is the normal presentation path; GDI remains a recovery option.
A system or audio-driver stall can still interrupt sound. Report problems
without attaching ROM files, prepared cartridges or RAM dumps.

## Development

See [BUILDING.md](BUILDING.md) to build from your own ROM. Generated C++
translations and IR are rebuilt locally and excluded from Git. Public CI runs
the checks that do not need a game ROM.

Launcher translation strings are in `launcher/Localization.cs`. Display
translations are separate from persisted controller/graphics values and the
native launch protocol.

## Credits and license

Original project code is licensed under **MIT**. External components, firmware
and original artwork retain their own rights/licenses: [NOTICE.md](NOTICE.md).

- SDL3, .NET and Vortice.DirectInput for Windows input and launcher services.
- MAME's SCSP/315-5881 references, with their BSD notices preserved.
- Ghidra for analysis and translation generation during development.
- [REA](https://github.com/morluto/rea) as a research tool.
- [Snowboard Kids: Recompiled](https://github.com/cdlewis/snowboardkids-recomp)
  as a reference for presentation and ROM selection. This is not its fork;
  N64Recomp/RT64 are not used to run this Sega ST-V game.

Unofficial project, not affiliated with TECMO or Sega's rights holders.
