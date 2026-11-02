# Development contract

- Keep the Windows native port; do not replace it with a MAME launcher.
- Exactly one thread owns the mutable NativeSession and all original CPU
  contexts. SDL and D3D11/DXGI remain on the window thread. Preserve immutable
  video snapshots, bounded queues, input focus/TTL and graceful stop/join.
- Never commit game ROM, mapped cart, EPROM, sound-driver extracts, generated
  translations/IR, full RAM/VRAM captures, player data or credentials. Firmware
  exceptions are explicit and hash-pinned in tools/audit_publication.py.
- Keep source and binary package separate. A playable release must start from
  a clean folder, request the user's ROM and include no imported game data.
- Preserve original/third-party notices. MIT does not relicense Sega/TECMO data.
- Do not claim complete hardware/controller/tournament fidelity from synthetic
  tests. State what was actually tested.
