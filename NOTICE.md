# Scope of the project license

The MIT license covers the original host-side implementation, launcher, tools
and documentation contributed to this project. It does not relicense the
original game, firmware, artwork, trademarks, or third-party components.

- **TECMO World Cup '98 / World Soccer '98** and the title-screen artwork belong
  to their respective rights holders. The game ROM and prepared cartridge are
  not distributed. Every player supplies the supported game themselves.
- **Sega ST-V firmware** is supplied in `firmware/` at the project owner's
  explicit request. Its original rights remain with Sega; no MIT license or
  new redistribution permission is claimed for these firmware files. See
  `firmware/NOTICE.md` for exact file hashes and provenance.
- **SDL 3** is under the zlib license, reproduced at
  `launcher/Native/SDL-LICENSE.txt`.
- **SCSP and 315-5881 adaptations** retain their BSD-3-Clause notices in
  `native-port/third-party/`. Original authors include ElSemi, R. Belmont,
  Andreas Naive, Olivier Galibert and David Haywood.
- **Vortice.DirectInput** is obtained through NuGet under its own license;
  Microsoft .NET and Windows/DirectX components retain their own terms.

Ghidra and MAME are external development/reference tools. They are not CPU
interpreters used by the shipped native game executable. Snowboard Kids:
Recompiled is a reference for project presentation and ROM-onboarding design;
this repository is not its fork and does not claim its features or authorship.
