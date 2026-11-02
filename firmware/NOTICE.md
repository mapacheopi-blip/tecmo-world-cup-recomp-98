# Firmware supplied separately from MIT code

These are firmware data, not MIT-licensed project source. Original Sega rights
are retained. Their presence does not make them public-domain or grant a new
license to the original game or firmware.

| File | Bytes | SHA-256 |
|---|---:|---|
| `bios-jp1.bin` | 524288 | `ac778ec04aaa4df296d30743536da3de31281f8ae5c94d7be433dcc84e25d85b` |
| `resident-jp1.bin` | 16384 | `0420455ca4d92a7c75cac7d8722da401eac7758b49c76345b807002898b883d0` |

`bios-jp1.bin` is the owner's supplied ST-V jp1 BIOS (EPR-20091).
`resident-jp1.bin` is the 16 KiB resident firmware region used by the native
bootstrap. The original full 1 MiB RAM capture is deliberately not included:
the runtime consumes only this first region, and the rest contains game data.
The loader validates exact size and hash before using either firmware file.
