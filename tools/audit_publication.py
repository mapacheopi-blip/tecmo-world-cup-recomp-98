"""Fail closed on game data, private files or unexpected binaries in a publication."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = {
 'firmware/bios-jp1.bin': 'ac778ec04aaa4df296d30743536da3de31281f8ae5c94d7be433dcc84e25d85b',
 'firmware/resident-jp1.bin': '0420455ca4d92a7c75cac7d8722da401eac7758b49c76345b807002898b883d0',
}
BINARY = dict(FIRMWARE, **{
 'launcher/Native/SDL3.dll': '2b4b128d8d510cc2d442693cf848c050d700d43dc667afe9e5fd82b0570d667e',
 'launcher/Assets/title.jpg': '757373c21086155240902595888eea6767db74e96281858c2834c8a1c0c24800',
 'launcher/Assets/match.png': '49fd63cc550b136553c6539e73f289889097200520016b8ce64e0bc503656e57',
})
GAME_HASHES = {
 'f66cc955181720661a0334fe67fa5750ddf9758b', '5fcaf4257f3639cb3aa407d2936f616499a09d97',
 'd5689ac8aad63509febe9aa4077351be09b2d8d4', 'c387d03ba27580c62ac0bf780915fdf41552df6f',
 '310961a5f114fea8938a3f514dffd5231e910a5a', '2e4f293a0b6cc5b32694671cd4d9ea20381add88',
 '69538b162c56a1075007ab392630070fd067683a',
}
PRIVATE = {'datanative', 'data', 'original', 'work', 'runtime', 'evidence', 'ghidra',
           'generated', 'generated-sound', 'test-runs', 'verification', 'validation'}
def audit(package=None):
 base = Path(package).resolve() if package else ROOT
 if package:
  paths = [p for p in base.rglob('*') if p.is_file()]
 else:
  listing = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=base)
  paths = [base/p.decode('utf-8') for p in listing.split(b'\0') if p]
 failures=[];total=0;seen=set()
 for path in paths:
  name=path.relative_to(base).as_posix();lower=name.lower();data=path.read_bytes();total+=len(data);seen.add(name)
  if any(p.lower() in PRIVATE for p in path.relative_to(base).parts):failures.append(name+': private/generated directory')
  if path.name.lower() in {'cart.bin','boot-epr.bin','twcup98.zip','sound-driver.bin','native-workram.bin'}:failures.append(name+': game data filename')
  if hashlib.sha1(data).hexdigest() in GAME_HASHES:failures.append(name+': original game or full RAM content')
  allowed = BINARY.get(name) if not package else {
   'Documentos/bios-jp1.bin':FIRMWARE['firmware/bios-jp1.bin'],
   'Documentos/resident-profile.bin':FIRMWARE['firmware/resident-jp1.bin'],
  }.get(name)
  if allowed:
   if hashlib.sha256(data).hexdigest()!=allowed:failures.append(name+': pinned binary differs')
  elif package:
   if path.suffix.lower() in {'.bin','.zip','.rom','.sav','.bmp','.wav','.ic2','.ic8'}:failures.append(name+': unexpected data file')
  else:
   if path.suffix.lower() in {'.bin','.dll','.exe','.png','.jpg','.zip','.rom','.sav','.bmp','.wav','.log'}:failures.append(name+': unexpected binary/artifact')
   if b'\x00' in data:failures.append(name+': unexpected binary content')
  if not allowed and not package:
   text=data.decode('utf-8',errors='replace')
   if re.search(r'(?:ghp_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|AKIA[A-Z0-9]{16})',text):failures.append(name+': possible credential')
 if package:
  for required in ['TecmoWorldCupRecomp98.exe','Player/tecmo98-native-player.exe','Documentos/bios-jp1.bin','Documentos/resident-profile.bin']:
   if required not in seen:failures.append('Missing '+required)
 else:
  for required in BINARY:
   if required not in seen:failures.append('Missing '+required)
 result={'passed':not failures,'mode':'package' if package else 'source','files':len(paths),'bytes':total,'game_data_found':bool(failures),'failures':failures}
 print(json.dumps(result,indent=2))
 if failures:raise SystemExit(1)
 return result
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--package',type=Path);audit(parser.parse_args().package)
