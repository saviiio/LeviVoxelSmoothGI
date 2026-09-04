#!/usr/bin/env python3
import argparse, json, subprocess
from pathlib import Path

def build_id(path: Path) -> str:
    try:
        out=subprocess.check_output(['readelf','-n',str(path)],text=True,errors='replace')
    except Exception:
        return ''
    for line in out.splitlines():
        if 'Build ID:' in line:
            return line.split('Build ID:',1)[1].strip().lower()
    return ''

def occurrences(blob: bytes, pat: bytes):
    result=[]; start=0
    while True:
        i=blob.find(pat,start)
        if i<0: break
        result.append(i); start=i+1
    return result

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('libminecraftpe')
    ap.add_argument('--profile',default=str(Path(__file__).resolve().parents[1]/'generated/current_minecraft_profile.json'))
    args=ap.parse_args()
    lib=Path(args.libminecraftpe)
    profile=json.loads(Path(args.profile).read_text())
    blob=lib.read_bytes()
    actual=build_id(lib)
    expected=profile['build_id'].lower()
    print(f'Build ID expected={expected} actual={actual or "<unknown>"}')
    ok=(actual==expected)
    offsets=profile.get('direct_plt_offsets', {})
    for name,sig in profile['direct_plt_signatures'].items():
        pat=bytes.fromhex(sig)
        hits=occurrences(blob,pat)
        expected_off=int(offsets[name],0) if name in offsets else None
        offset_ok=(expected_off is None or (expected_off + len(pat) <= len(blob) and blob[expected_off:expected_off+len(pat)] == pat))
        print(f'{name:28s} matches={len(hits)} offsets={[hex(x) for x in hits]} profile_offset={hex(expected_off) if expected_off is not None else "<none>"} offset_ok={offset_ok}')
        ok &= len(hits)==1 and offset_ok
    raise SystemExit(0 if ok else 1)

if __name__=='__main__': main()
