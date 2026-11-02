"""Reconstruct ignored build inputs from the player's own supported ROM."""
import argparse
import hashlib
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHIPS = [(1048576, 'f66cc955181720661a0334fe67fa5750ddf9758b'),
         (4194304, '5fcaf4257f3639cb3aa407d2936f616499a09d97'),
         (4194304, 'd5689ac8aad63509febe9aa4077351be09b2d8d4'),
         (4194304, 'c387d03ba27580c62ac0bf780915fdf41552df6f'),
         (4194304, '310961a5f114fea8938a3f514dffd5231e910a5a')]

def prepare(rom):
    found = {}
    with zipfile.ZipFile(rom) as archive:
        if len(archive.infolist()) > 512:
            raise ValueError('Unexpectedly large ZIP directory')
        for entry in archive.infolist():
            if entry.file_size not in (1048576, 4194304):
                continue
            data = archive.read(entry)
            key = (len(data), hashlib.sha1(data).hexdigest())
            if key in CHIPS:
                found[key] = data
    if any(chip not in found for chip in CHIPS):
        raise ValueError('ROM does not match JUET 980410 V1.000')
    eprom = found[CHIPS[0]]
    cart = bytearray(0x3000000)
    cart[1:0x200000:2] = eprom
    cart[0x200000:0x300000] = eprom
    cart[0x300000:0x400000] = eprom
    for i, chip in enumerate(CHIPS[1:]):
        data = found[chip]
        start = 0x400000 + i*0x400000
        cart[start:start+len(data):2] = data[1::2]
        cart[start+1:start+len(data):2] = data[0::2]
    assert hashlib.sha1(cart).hexdigest() == '2e4f293a0b6cc5b32694671cd4d9ea20381add88'
    sound = cart[14456420:14456420+23920]
    assert hashlib.sha256(sound).hexdigest() == '5e7f7b6b2275afaffe351267fbd382a949b12250a6feabfb36f53b030b2d3000'
    bios = (ROOT/'firmware/bios-jp1.bin').read_bytes()
    assert hashlib.sha1(bios).hexdigest() == 'eff0f54c70bce05ff3a289bf30b1027e1c8cd117'
    swapped = bytearray(bios)
    swapped[0::2], swapped[1::2] = bios[1::2], bios[0::2]
    work = ROOT/'work'
    work.mkdir(exist_ok=True)
    for name, data in [('boot-epr.bin', eprom), ('cart.bin', cart),
                       ('main-expanded.bin', eprom[0x4000:0x80000]),
                       ('sound-driver.bin', sound), ('bios-jp1-be.bin', swapped)]:
        (work/name).write_bytes(data)
    print('Verified game inputs prepared locally in work/; never commit this directory.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    prepare(parser.parse_args().rom)
