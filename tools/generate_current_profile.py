#!/usr/bin/env python3
from pathlib import Path
import argparse, json, subprocess
ITEM_NAMES=[
 "ItemInHandColor","ItemInHandColorGlint","ItemInHandTextured",
 "ItemInHandPrepass","ItemInHandPrepassGlint","ItemInHandPrepassTextured",
 "ItemInHandForwardPBR","ItemInHandForwardPBRGlint","ItemInHandForwardPBRTextured"]
def fnv(data):
 h=0xcbf29ce484222325
 for c in data: h=((h^c)*0x100000001b3)&0xffffffffffffffff
 return h
def fragments(path):
 b=path.read_bytes();pos=0;seen=[]
 while True:
  i=b.find(b'#version 310 es',pos)
  if i<0:return seen
  j=b.find(b'\0',i)
  if j<0:return seen
  s=b[i:j];pos=j+1
  if b'void main' not in s or (b'bgfx_Frag' not in s and b'gl_Frag' not in s):continue
  h=fnv(s)
  if h not in seen:seen.append(h)
def buildid(lib):
 t=subprocess.check_output(['readelf','-n',str(lib)],text=True,errors='ignore')
 return next((x.split('Build ID:')[1].strip() for x in t.splitlines() if 'Build ID:' in x),'unknown')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('materials',type=Path);ap.add_argument('libminecraftpe',type=Path);ap.add_argument('header',type=Path);ap.add_argument('json',type=Path);a=ap.parse_args()
 groups={
  'Deferred':fragments(a.materials/'DeferredShading.material.bin'),
  'RenderChunkForward':fragments(a.materials/'RenderChunkForwardPBR.material.bin'),
  'Ssr':fragments(a.materials/'ScreenSpaceReflections.material.bin'),
  'Item':[]}
 for n in ITEM_NAMES:
  for h in fragments(a.materials/(n+'.material.bin')):
   if h not in groups['Item']:groups['Item'].append(h)
 bid=buildid(a.libminecraftpe)
 a.header.parent.mkdir(parents=True,exist_ok=True);a.json.parent.mkdir(parents=True,exist_ok=True)
 lines=['#pragma once','#include <array>','#include <cstdint>','namespace lvsgi::profile {',f'inline constexpr char kMinecraftBuildId[] = "{bid}";']
 for name,vals in groups.items():
  lines.append(f'inline constexpr std::array<std::uint64_t, {len(vals)}> k{name}Hashes = {{')
  lines += [f'    0x{x:016x}ULL,' for x in vals];lines.append('};')
 lines.append('} // namespace lvsgi::profile');a.header.write_text('\n'.join(lines)+'\n')
 a.json.write_text(json.dumps({'build_id':bid,**{k:[f'{x:016x}' for x in v] for k,v in groups.items()}},indent=2)+'\n')
 print('Build ID',bid,', '.join(f'{k}={len(v)}' for k,v in groups.items()))
if __name__=='__main__':main()
