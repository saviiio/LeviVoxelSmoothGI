#include "lvsgi/ShaderPatcher.hpp"
#include "lvsgi/Config.hpp"
#include "lvsgi/Sha256.hpp"
#include <iostream>
#include <stdexcept>
using namespace lvsgi;
static void req(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){
  Sha256 h; h.update("abc"); req(Sha256::hex(h.finish())=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","sha256");
  Config c; c.voxelX=9999; c.voxelY=-1; c.giDecay=4.0f; c.prewarmProgramsPerFrame=-5; sanitizeConfig(c);
  req(c.version==9 && c.voxelX==128 && c.voxelY==16,"config sanitize dimensions");
  req(c.giDecay==0.99f && c.prewarmProgramsPerFrame==0,"config sanitize ranges");
  c.deferShaderCompilation=true; sanitizeConfig(c); req(!c.deferShaderCompilation,"unsafe shader deferral must stay disabled");
  ShaderPatcher p;
  std::string d=R"(#version 310 es
layout(binding=13,std430) buffer s_zLights{uint z[];};
uniform sampler2D s_ColorMetalnessSubsurface;uniform usampler2D s_EmissiveAmbientLinearRoughness;uniform vec4 SubPixelOffset;uniform sampler3D s_SkyAmbientSamples;uniform sampler2D s_SceneDepth;uniform sampler2D s_Normal;uniform mat4 u_invView;uniform mat4 u_invProj;uniform vec4 DirectionalLightSourceWorldSpaceDirection;in vec3 v_projPosition;in vec4 v_texcoord0;layout(location=0)out vec4 bgfx_FragColor;void main(){bgfx_FragColor=vec4(1);})";
  auto a=p.patch(0x8B30,d,16); req(!a.changed&&a.kind==PatchKind::None,"unknown deferred lookalike must not be patched"); req(a.source==d,"unknown deferred source changed");
  // A runtime preamble/define can change the full source hash even though this is
  // still the known DeferredShading material family. The strict structural
  // fingerprint must continue to patch it.
  std::string knownDeferred=R"(#version 310 es
// runtime-generated define variation: hash intentionally not in the profile
layout(binding=13,std430) buffer s_zLights{uint z[];};
uniform sampler2D s_ColorMetalnessSubsurface;uniform usampler2D s_EmissiveAmbientLinearRoughness;uniform sampler3D s_SkyAmbientSamples;uniform sampler2D s_SceneDepth;uniform sampler2D s_Normal;uniform mat4 u_invView;uniform mat4 u_invProj;uniform vec4 DirectionalLightSourceWorldSpaceDirection;uniform vec4 SubPixelOffset;uniform vec4 WorldOrigin;uniform vec4 QuantizationParameters;in vec3 v_projPosition;in vec4 v_texcoord0;layout(location=0)out vec4 bgfx_FragColor;void main(){bgfx_FragColor=vec4(1);})";
  auto da=p.patch(0x8B30,knownDeferred,16); req(da.changed&&da.kind==PatchKind::DeferredLighting,"strict deferred runtime variant must be patched");
  req(da.source.find("leviWriteCamera")!=std::string::npos,"deferred camera writer missing");
  req(da.source.find("leviFrameStamp")!=std::string::npos,"SSBO frame serial missing");
  req(da.source.find("intBitsToUint")!=std::string::npos,"signed camera encoding missing");
  req(da.source.find("gl_FragCoord.x < 1.5")==std::string::npos,"camera must not depend on one bottom-left pixel");
  std::string knownForward=R"(#version 310 es
struct PBRTextureData{vec4 x;};uniform mat4 u_invView;uniform mat4 u_view;uniform sampler2D s_PBRData;layout(std430,binding=10)buffer s_VoxelBuffer{uint a[];};layout(std430,binding=11)buffer s_GpuEntryBuffer{uint b[];};uniform sampler2D s_MatTexture;uniform vec4 WorldOrigin;in vec3 v_worldPos;in vec3 v_normal;flat in int v_pbrTextureId;layout(location=0)out vec4 bgfx_FragColor;void main(){bgfx_FragColor=vec4(1);})";
  auto fa=p.patch(0x8B30,knownForward,16); req(fa.changed&&fa.kind==PatchKind::ForwardWorldCapture,"strict forward runtime variant must be patched");
  std::string s=R"(#version 310 es
uniform vec4 SSRRoughnessCutoffParams;uniform sampler2D s_GbufferDepth;uniform sampler2D s_GbufferNormal;layout(location=0)out vec4 bgfx_FragData0;void main(){bgfx_FragData0=vec4(1);})";
  auto b=p.patch(0x8B30,s,8); req(!b.changed&&b.kind==PatchKind::None,"unknown ssr lookalike must not be patched"); req(b.source==s,"unknown ssr source changed");
  std::cout<<"OK\n";
}
