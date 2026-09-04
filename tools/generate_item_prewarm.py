#!/usr/bin/env python3
"""Generate the runtime ItemInHand pipeline prewarm database from current RenderDragon material binaries."""
from pathlib import Path
import argparse, hashlib, struct
NAMES = [
    "ItemInHandColor", "ItemInHandColorGlint", "ItemInHandTextured",
    "ItemInHandPrepass", "ItemInHandPrepassGlint", "ItemInHandPrepassTextured",
    "ItemInHandForwardPBR", "ItemInHandForwardPBRGlint", "ItemInHandForwardPBRTextured",
]
def stages(path: Path):
    b=path.read_bytes(); pos=0
    while True:
        i=b.find(b"#version 310 es",pos)
        if i<0: return
        j=b.find(b"\0",i)
        if j<0: return
        s=b[i:j]; pos=j+1
        if b"void main" not in s: continue
        fragment = b"bgfx_Frag" in s or b"gl_Frag" in s
        vertex = b"gl_Position" in s and not fragment
        if vertex: yield 0,s
        elif fragment: yield 1,s
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("materials",type=Path,help="directory containing ItemInHand*.material.bin")
    ap.add_argument("output",type=Path)
    a=ap.parse_args()
    shader_index={}; shader_list=[]; programs=[]
    for name in NAMES:
        seq=[]
        for stage,source in stages(a.materials/(name+".material.bin")):
            key=(stage,hashlib.sha256(source).digest())
            if key not in shader_index:
                shader_index[key]=len(shader_list); shader_list.append((stage,source))
            seq.append(shader_index[key])
        if len(seq)%2: raise SystemExit(f"odd stage count in {name}: {len(seq)}")
        for i in range(0,len(seq),2):
            v,f=seq[i],seq[i+1]
            if shader_list[v][0]!=0 or shader_list[f][0]!=1:
                raise SystemExit(f"stage order is not vertex/fragment in {name}, pair {i//2}")
            programs.append((v,f))
    programs=list(dict.fromkeys(programs))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open("wb") as f:
        f.write(b"LVPR1\0\0\0"); f.write(struct.pack("<II",len(shader_list),len(programs)))
        for stage,source in shader_list:
            f.write(struct.pack("<BI",stage,len(source))); f.write(source)
        for v,frag in programs: f.write(struct.pack("<II",v,frag))
    print(f"{a.output}: {len(shader_list)} unique shaders, {len(programs)} unique programs")
if __name__=="__main__": main()
