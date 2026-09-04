#include "lvsgi/ShaderPatcher.hpp"
#include "CurrentMinecraftProfile.hpp"
#include <array>
#include <cctype>
#include <string_view>
#include <vector>
#include <algorithm>

namespace lvsgi {
namespace {
constexpr unsigned kFragmentShader = 0x8B30u;

template <class A> bool hasHash(const A& a, std::uint64_t h) {
    return std::find(a.begin(), a.end(), h) != a.end();
}
bool hasAll(const std::string& s, std::initializer_list<const char*> xs) {
    for (auto* x : xs) if (s.find(x) == std::string::npos) return false;
    return true;
}
bool renameMain(std::string& s) {
    std::size_t p = 0;
    while ((p = s.find("void", p)) != std::string::npos) {
        if (p && (std::isalnum((unsigned char)s[p-1]) || s[p-1]=='_')) { p += 4; continue; }
        std::size_t q = p + 4;
        while (q < s.size() && std::isspace((unsigned char)s[q])) ++q;
        if (s.compare(q, 4, "main") != 0) { p = q; continue; }
        std::size_t e = q + 4;
        if (e < s.size() && (std::isalnum((unsigned char)s[e]) || s[e]=='_')) { p=e; continue; }
        while (e < s.size() && std::isspace((unsigned char)s[e])) ++e;
        if (e >= s.size() || s[e] != '(') { p=e; continue; }
        s.replace(q, 4, "leviOriginalMain");
        return true;
    }
    return false;
}
std::string outName(const std::string& s) {
    if (s.find("bgfx_FragColor") != std::string::npos) return "bgfx_FragColor";
    if (s.find("bgfx_FragData0") != std::string::npos) return "bgfx_FragData0";
    return {};
}
std::string replaceAll(std::string s, std::string_view a, std::string_view b) {
    std::size_t p=0; while((p=s.find(a,p))!=std::string::npos){s.replace(p,a.size(),b);p+=b.size();} return s;
}

const char* kCommon = R"GLSL(

// ---- LeviVoxelSmoothGI injected block ----
layout(std430, binding = __BINDING__) buffer LeviVoxelSmoothGIStorage { coherent uint leviData[]; };
uniform ivec4 uLeviVoxelDims;
uniform int uLeviFrameParity;
uniform float uLeviGiStrength;
uniform float uLeviReflectionStrength;
uniform float uLeviReflectionRange;

const int LEVI_HEADER = 16;
const vec3 LEVI_DIRS[6] = vec3[6](
    vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));

int leviModI(int a,int b){ int m=a%b; return m<0?m+b:m; }
int leviFloorDivI(int a,int b){ return a>=0?a/b:-((-a+b-1)/b); }
ivec3 leviFloorDiv3(ivec3 a,int b){ return ivec3(leviFloorDivI(a.x,b),leviFloorDivI(a.y,b),leviFloorDivI(a.z,b)); }
int leviN(){ return uLeviVoxelDims.x*uLeviVoxelDims.y*uLeviVoxelDims.z; }
int leviIndex(ivec3 w,ivec3 d){ ivec3 q=ivec3(leviModI(w.x,d.x),leviModI(w.y,d.y),leviModI(w.z,d.z)); return q.x+d.x*(q.y+d.y*q.z); }
uint leviTag(ivec3 p){ uint h=2166136261u; h=(h^uint(p.x))*16777619u; h=(h^uint(p.y))*16777619u; h=(h^uint(p.z))*16777619u; return (h&0x7fffffffu)|1u; }
int leviTagOff(){return LEVI_HEADER;} int leviMaskOff(){return LEVI_HEADER+leviN();}
int leviFaceOff(int pair){return LEVI_HEADER+(2+pair)*leviN();}
int leviSourceOff(int pair){return LEVI_HEADER+(5+pair)*leviN();}
int leviGiOff(int parity,int pair){return LEVI_HEADER+(8+parity*3+pair)*leviN();}
ivec3 leviLevelDims(int level){int s=1<<level;return (uLeviVoxelDims.xyz+ivec3(s-1))/s;}
int leviHierarchyStart(){return LEVI_HEADER+14*leviN();}
int leviHierarchyOff(int level){int o=leviHierarchyStart();for(int l=1;l<4;l++){if(l==level)return o;ivec3 d=leviLevelDims(l);o+=2*d.x*d.y*d.z;}return o;}
uint leviPack565(vec3 c){c=clamp(c*0.25,0.0,1.0);uvec3 q=uvec3(round(c*vec3(31,63,31)));return q.x|(q.y<<5u)|(q.z<<11u);}
vec3 leviUnpack565(uint p){return 4.0*vec3(float(p&31u)/31.0,float((p>>5u)&63u)/63.0,float((p>>11u)&31u)/31.0);}
uint leviGetHalf(uint p,int h){return h==0?(p&65535u):(p>>16u);}
vec3 leviReadPairColor(int off,int idx,int face){uint p=leviData[off+(face>>1)*leviN()+idx];return leviUnpack565(leviGetHalf(p,face&1));}
void leviAtomicHalf(int address,int half,uint value){uint shift=uint(half*16);uint mask=65535u<<shift;for(int i=0;i<12;i++){uint old=leviData[address];uint neu=(old&~mask)|((value&65535u)<<shift);if(atomicCompSwap(leviData[address],old,neu)==old)break;}}
int leviFaceFromNormal(vec3 n){vec3 a=abs(n);if(a.x>=a.y&&a.x>=a.z)return n.x>=0.0?0:1;if(a.y>=a.z)return n.y>=0.0?2:3;return n.z>=0.0?4:5;}
int leviOpp(int f){return f^1;}

bool leviEnsureSolidCell(ivec3 wc,int idx){
    uint tag=leviTag(wc); int addr=leviTagOff()+idx; uint cur=leviData[addr];
    for(int k=0;k<16;k++){
        if(cur==tag)return true;
        if((cur&0x80000000u)!=0u){cur=leviData[addr];continue;}
        uint got=atomicCompSwap(leviData[addr],cur,tag|0x80000000u);
        if(got==cur){
            int n=leviN(); for(int a=1;a<14;a++)leviData[LEVI_HEADER+a*n+idx]=0u;
            memoryBarrierBuffer(); atomicExchange(leviData[addr],tag); return true;
        }
        cur=got;
    }
    return false;
}
void leviMarkHierarchy(ivec3 wc){
    for(int l=1;l<4;l++){int s=1<<l;ivec3 c=leviFloorDiv3(wc,s);ivec3 d=leviLevelDims(l);int idx=leviIndex(c,d);int o=leviHierarchyOff(l);int n=d.x*d.y*d.z;atomicExchange(leviData[o+idx],leviTag(c));atomicExchange(leviData[o+n+idx],1u);}
}
uint leviFrameStamp(){return max(leviData[15],1u);}
void leviMarkFrame(int slot){atomicMax(leviData[slot],leviFrameStamp());}
void leviWriteCamera(vec3 camera){
    uint stamp=leviFrameStamp();
    uint old=atomicMax(leviData[3],stamp);
    if(old<stamp){
        ivec3 c=ivec3(floor(camera));
        atomicExchange(leviData[0],intBitsToUint(c.x));
        atomicExchange(leviData[1],intBitsToUint(c.y));
        atomicExchange(leviData[2],intBitsToUint(c.z));
        memoryBarrierBuffer();
        atomicMax(leviData[4],stamp);
    }
}
void leviCaptureSurface(vec3 worldPos,vec3 normal,vec3 radiance,vec3 camera){
    leviMarkFrame(5);
    ivec3 wc=ivec3(floor(worldPos));ivec3 cc=ivec3(floor(camera));ivec3 halfD=uLeviVoxelDims.xyz/2;
    if(any(greaterThan(abs(wc-cc),halfD+ivec3(2))))return;
    int idx=leviIndex(wc,uLeviVoxelDims.xyz);if(!leviEnsureSolidCell(wc,idx))return;
    int face=leviFaceFromNormal(normal);atomicOr(leviData[leviMaskOff()+idx],1u<<uint(face));
    vec3 source=max(radiance,vec3(0.0));uint packed=leviPack565(source);
    // Six physical faces remain independent. GI is seeded into the exposed air
    // voxel outside the face, matching the Java floodfill's air-host topology.
    leviAtomicHalf(leviFaceOff(face>>1)+idx,face&1,packed);
    ivec3 air=wc+ivec3(LEVI_DIRS[face]);int ai=leviIndex(air,uLeviVoxelDims.xyz);
    if(leviEnsureSolidCell(air,ai)&&leviData[leviMaskOff()+ai]==0u){
        leviAtomicHalf(leviSourceOff(face>>1)+ai,face&1,packed);
        leviMarkFrame(6);
    }
    leviMarkHierarchy(wc);
}
bool leviBaseOccupied(ivec3 wc){int idx=leviIndex(wc,uLeviVoxelDims.xyz);return leviData[leviTagOff()+idx]==leviTag(wc)&&leviData[leviMaskOff()+idx]!=0u;}
bool leviOccupied(ivec3 c,int level){
    if(level==0)return leviBaseOccupied(c);ivec3 d=leviLevelDims(level);int idx=leviIndex(c,d);int o=leviHierarchyOff(level);int n=d.x*d.y*d.z;return leviData[o+idx]==leviTag(c)&&leviData[o+n+idx]!=0u;
}
vec3 leviFaceColor(ivec3 wc,int face){int idx=leviIndex(wc,uLeviVoxelDims.xyz);if(leviData[leviTagOff()+idx]!=leviTag(wc))return vec3(0);uint mask=leviData[leviMaskOff()+idx];int f=face;if((mask&(1u<<uint(f)))==0u){for(int i=0;i<6;i++)if((mask&(1u<<uint(i)))!=0u){f=i;break;}}return leviReadPairColor(leviFaceOff(0),idx,f);}
vec3 leviGiDirection(ivec3 wc,int face){int idx=leviIndex(wc,uLeviVoxelDims.xyz);if(leviData[leviTagOff()+idx]!=leviTag(wc)||leviData[leviMaskOff()+idx]!=0u)return vec3(0);return leviReadPairColor(leviGiOff(uLeviFrameParity,0),idx,face);}
vec3 leviGiAtAir(ivec3 wc,vec3 normal){if(leviBaseOccupied(wc))return vec3(0);vec3 s=vec3(0);float wsum=0.0;for(int i=0;i<6;i++){float w=max(dot(-LEVI_DIRS[i],normal),0.0);if(w>0.0){s+=leviGiDirection(wc,i)*w;wsum+=w;}}return s/max(wsum,0.001);}
vec3 leviSampleGi(vec3 worldPos,vec3 normal){
    // Like GetComplexLightVolume: only the surface-side cell and axial paths are
    // considered. No diagonal corner tap is allowed to leak light through walls.
    ivec3 base=ivec3(floor(worldPos+normal*0.58));vec3 sum=leviGiAtAir(base,normal)*0.58;float weight=0.58;
    for(int a=0;a<6;a++){ivec3 q=base+ivec3(LEVI_DIRS[a]);if(!leviBaseOccupied(q)){float w=0.07;sum+=leviGiAtAir(q,normal)*w;weight+=w;}}
    return sum/max(weight,0.001);
}
float leviAdvanceToBoundary(vec3 ro,vec3 rd,float t,float cellSize){vec3 p=ro+rd*t;vec3 c=floor(p/cellSize);vec3 b=(c+step(vec3(0),rd))*cellSize;vec3 dt=(b-p)/rd;dt=vec3(rd.x==0.0?1e20:dt.x,rd.y==0.0?1e20:dt.y,rd.z==0.0?1e20:dt.z);float d=min(dt.x,min(dt.y,dt.z));return t+max(d,0.0008);}
int leviHitFace(vec3 rd,vec3 p){vec3 q=fract(p);vec3 e=min(q,1.0-q);int axis=e.x<=e.y&&e.x<=e.z?0:(e.y<=e.z?1:2);if(axis==0)return rd.x>0.0?1:0;if(axis==1)return rd.y>0.0?3:2;return rd.z>0.0?5:4;}
vec4 leviTraceHdda(vec3 ro,vec3 rd,float maxDist){float t=0.04;int level=3;for(int it=0;it<144;it++){if(t>=maxDist)break;float s=float(1<<level);vec3 p=ro+rd*t;ivec3 c=ivec3(floor(p/s));if(!leviOccupied(c,level)){t=leviAdvanceToBoundary(ro,rd,t,s);if(level<3)level++;continue;}if(level>0){level--;continue;}ivec3 wc=ivec3(floor(p));int face=leviHitFace(rd,p);return vec4(leviFaceColor(wc,face),t);}return vec4(0);}
// ---- end injected block ----
)GLSL";

std::string commonFor(int binding) {
    return replaceAll(kCommon, "__BINDING__", std::to_string(binding));
}
}

std::uint64_t ShaderPatcher::fnv1a64(const std::string& s){std::uint64_t h=0xcbf29ce484222325ULL;for(unsigned char c:s){h^=c;h*=0x100000001b3ULL;}return h;}
int ShaderPatcher::chooseBinding(const std::string& s,int maxBindings) const {
    std::vector<int> used;
    std::size_t p=0;
    while((p=s.find("binding",p))!=std::string::npos){auto e=s.find('=',p+7);if(e==std::string::npos){p+=7;continue;}++e;while(e<s.size()&&std::isspace((unsigned char)s[e]))++e;int v=0;bool any=false;while(e<s.size()&&std::isdigit((unsigned char)s[e])){any=true;v=v*10+(s[e++]-'0');}if(any)used.push_back(v);p=e;}
    for(int b=maxBindings-1;b>=0;b--){
        if(std::find(used.begin(),used.end(),b)==used.end()) return b;
    }
    return -1;
}
PatchResult ShaderPatcher::patch(unsigned type,const std::string& source,int maxBindings) const {
    PatchResult r{source,PatchKind::None,-1,false}; if(type!=kFragmentShader||source.find("#version 310 es")==std::string::npos)return r;
    auto h=fnv1a64(source);
    // RenderDragon may pass the material shader to glShaderSource with a small
    // runtime preamble/define variation.  Hashing the concatenated runtime
    // source therefore cannot be the only gate: beta.8.4 could see the correct
    // Deferred shader yet reject it solely because the full-source hash changed.
    // Keep exact hashes as the strongest profile match, but accept only very
    // strict structural fingerprints for the same known material families.
    const bool deferredSig=hasAll(source,{"s_ColorMetalnessSubsurface","s_EmissiveAmbientLinearRoughness","s_SkyAmbientSamples","s_SceneDepth","s_Normal","u_invView","u_invProj","DirectionalLightSourceWorldSpaceDirection","v_projPosition","v_texcoord0","SubPixelOffset","WorldOrigin","QuantizationParameters"});
    const bool forwardSig=hasAll(source,{"PBRTextureData","v_worldPos","v_normal","u_invView","u_view","s_PBRData","s_VoxelBuffer","s_GpuEntryBuffer","s_MatTexture","WorldOrigin","v_pbrTextureId"});
    const bool ssrSig=hasAll(source,{"SSRRoughnessCutoffParams","s_GbufferDepth","s_GbufferNormal","bgfx_FragData0"});
    const bool deferredExact=hasHash(profile::kDeferredHashes,h);
    const bool forwardExact=hasHash(profile::kRenderChunkForwardHashes,h);
    bool deferred=deferredSig;
    bool forward=forwardSig;
    // Disabling native SSR is not required for GI activation, so retain the
    // exact profile gate for this destructive replacement.
    bool ssr=ssrSig && hasHash(profile::kSsrHashes,h);
    (void)deferredExact;
    (void)forwardExact;
    if(ssr){std::string s=source;if(!renameMain(s))return r;auto out=outName(s);if(out.empty())return r;s += "\nvoid main(){ leviOriginalMain(); "+out+"=vec4(0.0); }\n";r.source=std::move(s);r.kind=PatchKind::DisableNativeSsr;r.changed=true;return r;}
    if(!deferred && !forward)return r;
    int binding=chooseBinding(source,maxBindings);if(binding<0)return r;std::string s=source;if(!renameMain(s))return r;auto out=outName(s);if(out.empty())return r;s += commonFor(binding);
    if(deferred){
        s += "\nvoid main(){\n  leviOriginalMain();\n  vec2 leviUv=v_texcoord0.xy;\n  vec3 leviCamera=(u_invView*vec4(0.0,0.0,0.0,1.0)).xyz;\n  leviWriteCamera(leviCamera);\n  leviMarkFrame(7);\n  float leviDepth=texture(s_SceneDepth,leviUv).x;\n  if(leviDepth<0.999999){\n    vec4 leviV=u_invProj*vec4(v_projPosition.xy+vec2(SubPixelOffset.x,-SubPixelOffset.y),leviDepth*2.0-1.0,1.0); leviV/=max(abs(leviV.w),1e-6);\n    vec3 leviWorld=(u_invView*vec4(leviV.xyz,1.0)).xyz;\n    vec2 leviOct=texture(s_Normal,leviUv).xy; vec3 leviN=vec3(leviOct,1.0-abs(leviOct.x)-abs(leviOct.y));\n    if(leviN.z<0.0)leviN.xy=(1.0-abs(leviN.yx))*sign(leviN.xy); leviN=normalize(leviN);\n    vec4 leviCms=texture(s_ColorMetalnessSubsurface,leviUv); vec3 leviAlbedo=max(leviCms.rgb,vec3(0.04));\n    vec3 leviNativeSource=max("+out+".rgb,vec3(0.0));\n    vec3 leviBounceSource=leviNativeSource*mix(vec3(0.72),sqrt(leviAlbedo),vec3(0.72));\n    if(((int(gl_FragCoord.x)|int(gl_FragCoord.y))&1)==0) leviCaptureSurface(leviWorld,leviN,leviBounceSource,leviCamera);\n    vec3 leviGi=leviSampleGi(leviWorld,leviN)*leviAlbedo*uLeviGiStrength;\n    if(max(leviGi.r,max(leviGi.g,leviGi.b))>0.0005) leviMarkFrame(8);\n    "+out+".rgb+=leviGi;\n    uvec4 leviER=texelFetch(s_EmissiveAmbientLinearRoughness,ivec2(vec2(textureSize(s_EmissiveAmbientLinearRoughness,0))*leviUv),0);\n    uint leviPacked=leviER.x&65535u; float leviRough=float(leviPacked>>8u)/255.0; float leviMetal=clamp(2.007874*(leviCms.a-0.5019608),0.0,1.0);\n    vec3 leviVdir=normalize(leviCamera-leviWorld);\n    if(leviRough<0.92&&uLeviReflectionStrength>0.001){vec3 leviRdir=normalize(reflect(-leviVdir,leviN));vec4 leviHit=leviTraceHdda(leviWorld+leviN*0.08,leviRdir,uLeviReflectionRange);if(leviHit.a>0.0){float leviF=pow(1.0-max(dot(leviVdir,leviN),0.0),5.0);vec3 leviF0=mix(vec3(0.04),leviAlbedo,leviMetal);vec3 leviFres=leviF0+(1.0-leviF0)*leviF;float leviSpec=(1.0-leviRough)*(1.0-leviRough);float leviFade=1.0-clamp(leviHit.a/uLeviReflectionRange,0.0,1.0);"+out+".rgb+=leviHit.rgb*leviFres*(leviSpec*leviFade*uLeviReflectionStrength);}}\n  }\n}\n";
        r.kind=PatchKind::DeferredLighting;
    } else {
        s += "\nvoid main(){ leviOriginalMain(); vec3 leviCamera=(u_invView*vec4(0.0,0.0,0.0,1.0)).xyz; leviWriteCamera(leviCamera); leviCaptureSurface(v_worldPos,normalize(v_normal),max("+out+".rgb,vec3(0.0))*0.85,leviCamera); }\n";
        r.kind=PatchKind::ForwardWorldCapture;
    }
    r.source=std::move(s);r.ssboBinding=binding;r.changed=true;return r;
}
} // namespace lvsgi
