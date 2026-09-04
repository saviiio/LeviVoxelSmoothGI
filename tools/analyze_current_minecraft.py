#!/usr/bin/env python3
from pathlib import Path
import hashlib, subprocess, sys, json

def extracts(p):
 b=p.read_bytes(); m=b'#version 310 es'; pos=0; out=[]; seen=set()
 while True:
  i=b.find(m,pos)
  if i<0: break
  j=b.find(b'\0',i)
  if j<0: break
  s=b[i:j];pos=j+1
  if b'void main' not in s: continue
  h=hashlib.sha256(s).hexdigest()
  if h not in seen: seen.add(h);out.append((h,len(s),b'bgfx_Frag' in s))
 return out
root=Path(sys.argv[1])
lib=Path(sys.argv[2])
text=subprocess.check_output(['readelf','-n',str(lib)],text=True,errors='ignore')
bid=next((x.split('Build ID:')[1].strip() for x in text.splitlines() if 'Build ID:' in x),'unknown')
names=['DeferredShading.material.bin','RenderChunkForwardPBR.material.bin','ScreenSpaceReflections.material.bin','ItemInHandForwardPBR.material.bin']
print(json.dumps({'build_id':bid,'materials':{n:extracts(root/n) for n in names}},indent=2))
