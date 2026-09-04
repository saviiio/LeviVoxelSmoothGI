#include "lvsgi/VoxelRuntime.hpp"
#include "lvsgi/Log.hpp"
#include <EGL/egl.h>
#include <string>
#include <algorithm>
#include <cstring>
#include <array>

namespace lvsgi {
namespace {
const char* kCompute = R"GLSL(#version 310 es
layout(local_size_x=4,local_size_y=4,local_size_z=4) in;
layout(std430,binding=0) buffer VoxelData { coherent uint d[]; };
uniform ivec3 uDims; uniform int uParity; uniform int uFrameSerial; uniform float uDecay,uTurn,uBack;
const int H=16; const ivec3 D[6]=ivec3[6](ivec3(1,0,0),ivec3(-1,0,0),ivec3(0,1,0),ivec3(0,-1,0),ivec3(0,0,1),ivec3(0,0,-1));
int modp(int a,int b){int m=a%b;return m<0?m+b:m;} int N(){return uDims.x*uDims.y*uDims.z;}
int idx(ivec3 w){ivec3 q=ivec3(modp(w.x,uDims.x),modp(w.y,uDims.y),modp(w.z,uDims.z));return q.x+uDims.x*(q.y+uDims.y*q.z);} 
uint tag(ivec3 p){uint h=2166136261u;h=(h^uint(p.x))*16777619u;h=(h^uint(p.y))*16777619u;h=(h^uint(p.z))*16777619u;return (h&0x7fffffffu)|1u;}
int tagO(){return H;} int maskO(){return H+N();} int srcO(int pair){return H+(5+pair)*N();} int giO(int parity,int pair){return H+(8+parity*3+pair)*N();}
uint halfv(uint p,int h){return h==0?(p&65535u):(p>>16u);} uint pack565(vec3 c){c=clamp(c*.25,0.,1.);uvec3 q=uvec3(round(c*vec3(31,63,31)));return q.x|(q.y<<5u)|(q.z<<11u);} vec3 unpack565(uint p){return 4.*vec3(float(p&31u)/31.,float((p>>5u)&63u)/63.,float((p>>11u)&31u)/31.);}
vec3 readFace(int base,int i,int f){return unpack565(halfv(d[base+(f>>1)*N()+i],f&1));}
void writeHalf(int addr,int h,uint v){uint sh=uint(h*16);uint mask=65535u<<sh;for(int k=0;k<12;k++){uint old=d[addr];uint neu=(old&~mask)|((v&65535u)<<sh);if(atomicCompSwap(d[addr],old,neu)==old)break;}}
void writeFace(int base,int i,int f,vec3 c){writeHalf(base+(f>>1)*N()+i,f&1,pack565(c));}
int opp(int f){return f^1;}
void markFrame(int slot,uint serial){atomicMax(d[slot],serial);}
void clearCell(int i){for(int a=1;a<14;a++)d[H+a*N()+i]=0u;}
void ensureCell(ivec3 w,int i){uint t=tag(w);if(d[tagO()+i]==t)return;d[tagO()+i]=t;clearCell(i);}
vec3 propagatedFrom(ivec3 n,int direction,int prev){int ni=idx(n);if(d[tagO()+ni]!=tag(n)||d[maskO()+ni]!=0u)return vec3(0);return readFace(giO(prev,0),ni,direction);}
void main(){ivec3 l=ivec3(gl_GlobalInvocationID.xyz);if(any(greaterThanEqual(l,uDims)))return;uint serial=uint(max(uFrameSerial,1));if(d[4]!=serial)return;if(all(equal(l,ivec3(0))))markFrame(11,serial);ivec3 camera=ivec3(uintBitsToInt(d[0]),uintBitsToInt(d[1]),uintBitsToInt(d[2]));ivec3 origin=camera-uDims/2;ivec3 w=origin+l;int i=idx(w);ensureCell(w,i);uint mask=d[maskO()+i];int prev=uParity;int next=1-prev;if(mask!=0u){d[giO(next,0)+i]=0u;d[giO(next,1)+i]=0u;d[giO(next,2)+i]=0u;d[srcO(0)+i]=0u;d[srcO(1)+i]=0u;d[srcO(2)+i]=0u;return;}
// The Java backend computes a source in an air voxel by inspecting its six solid
// neighbors. Raster capture has already done that face selection, so each source
// channel here is the corresponding face entering this air cell.
vec3 inc[6];float incPeak=0.0;for(int f=0;f<6;f++){inc[f]=readFace(srcO(0),i,f)+propagatedFrom(w-D[f],f,prev);incPeak=max(incPeak,max(inc[f].r,max(inc[f].g,inc[f].b)));}if(incPeak>0.0005)markFrame(9,serial);
vec3 result[6];float norm=max(1.0+4.0*uTurn+uBack,0.001);for(int f=0;f<6;f++){vec3 sum=inc[f]+inc[opp(f)]*uBack;for(int q=0;q<6;q++)if(q!=f&&q!=opp(f))sum+=inc[q]*uTurn;vec3 fresh=sum*(uDecay/norm);vec3 old=readFace(giO(prev,0),i,f);result[f]=mix(old,fresh,.58);}
float outPeak=0.0;for(int f=0;f<6;f++)outPeak=max(outPeak,max(result[f].r,max(result[f].g,result[f].b)));if(outPeak>0.0005)markFrame(10,serial);
for(int pair=0;pair<3;pair++){uint lo=pack565(result[pair*2]);uint hi=pack565(result[pair*2+1]);d[giO(next,pair)+i]=lo|(hi<<16u);d[srcO(pair)+i]=0u;}
})GLSL";

GLuint compile(GLenum type,const char* src){GLuint s=glCreateShader(type);glShaderSource(s,1,&src,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){GLint n=0;glGetShaderiv(s,GL_INFO_LOG_LENGTH,&n);std::string log(std::max(1,n),'\0');glGetShaderInfoLog(s,(GLsizei)log.size(),nullptr,log.data());LVSGI_E("compute shader compile failed: %s",log.c_str());glDeleteShader(s);return 0;}return s;}
}

std::size_t VoxelRuntime::uintCount(const Config& c)const{
 const std::size_t n=(std::size_t)c.voxelX*c.voxelY*c.voxelZ;std::size_t total=16+14*n;
 for(int level=1;level<=3;level++){int s=1<<level;std::size_t x=(c.voxelX+s-1)/s,y=(c.voxelY+s-1)/s,z=(c.voxelZ+s-1)/s;total+=2*x*y*z;}return total;
}
bool VoxelRuntime::compileCompute(const Config&){GLuint s=compile(GL_COMPUTE_SHADER,kCompute);if(!s)return false;compute_=glCreateProgram();glAttachShader(compute_,s);glLinkProgram(compute_);glDeleteShader(s);GLint ok=0;glGetProgramiv(compute_,GL_LINK_STATUS,&ok);if(!ok){GLint n=0;glGetProgramiv(compute_,GL_INFO_LOG_LENGTH,&n);std::string log(std::max(1,n),'\0');glGetProgramInfoLog(compute_,(GLsizei)log.size(),nullptr,log.data());LVSGI_E("compute program link failed: %s",log.c_str());glDeleteProgram(compute_);compute_=0;return false;}uDims_=glGetUniformLocation(compute_,"uDims");uParity_=glGetUniformLocation(compute_,"uParity");uFrameSerial_=glGetUniformLocation(compute_,"uFrameSerial");uDecay_=glGetUniformLocation(compute_,"uDecay");uTurn_=glGetUniformLocation(compute_,"uTurn");uBack_=glGetUniformLocation(compute_,"uBack");return true;}
bool VoxelRuntime::ensure(const Config& c){if(buffer_&&dx_==c.voxelX&&dy_==c.voxelY&&dz_==c.voxelZ)return true;if(eglGetCurrentContext()==EGL_NO_CONTEXT)return false;shutdown();dx_=c.voxelX;dy_=c.voxelY;dz_=c.voxelZ;const std::size_t bytes=uintCount(c)*sizeof(std::uint32_t);glGenBuffers(1,&buffer_);glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer_);glBufferData(GL_SHADER_STORAGE_BUFFER,(GLsizeiptr)bytes,nullptr,GL_DYNAMIC_DRAW);void* mapped=glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,(GLsizeiptr)bytes,GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);if(!mapped){glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);shutdown();return false;}std::memset(mapped,0,bytes);if(glUnmapBuffer(GL_SHADER_STORAGE_BUFFER)!=GL_TRUE){glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);shutdown();return false;}glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);if(glGetError()!=GL_NO_ERROR||!compileCompute(c)){shutdown();return false;}LVSGI_I("voxel volume allocated: %dx%dx%d, 1 block/cell, %.2f MiB",dx_,dy_,dz_,double(bytes)/(1024.0*1024.0));return true;}
void VoxelRuntime::shutdown(){if(eglGetCurrentContext()!=EGL_NO_CONTEXT){if(compute_)glDeleteProgram(compute_);if(buffer_)glDeleteBuffers(1,&buffer_);}else if(compute_||buffer_){LVSGI_W("GPU context already gone during shutdown; skipping GL deletes");}compute_=buffer_=0;parity_=0;frame_=0;dispatches_=0;dx_=dy_=dz_=0;}
void VoxelRuntime::bindForProgram(int binding)const{if(buffer_&&binding>=0)glBindBufferBase(GL_SHADER_STORAGE_BUFFER,(GLuint)binding,buffer_);}
void VoxelRuntime::endFrame(const Config& c){
    frame_++;
    if(!buffer_||eglGetCurrentContext()==EGL_NO_CONTEXT)return;
    GLint oldGenericSsbo=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&oldGenericSsbo);
    GLint oldBase0=0;glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,0,&oldBase0);
    if(compute_&&c.directionalFloodfillGi){
        GLint oldProgram=0;glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,buffer_);
        /* Fragment-stage SSBO writes that seeded face/source data must be visible to this compute dispatch. */
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glUseProgram(compute_);glUniform3i(uDims_,dx_,dy_,dz_);glUniform1i(uParity_,parity_);glUniform1i(uFrameSerial_,static_cast<GLint>(frame_&0x7fffffffu));glUniform1f(uDecay_,c.giDecay);glUniform1f(uTurn_,c.giTurn);glUniform1f(uBack_,c.giBackTurn);
        glDispatchCompute((GLuint)((dx_+3)/4),(GLuint)((dy_+3)/4),(GLuint)((dz_+3)/4));dispatches_++;
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);parity_=1-parity_;glUseProgram((GLuint)oldProgram);
    }
    /* Publish the serial for the *next* raster frame in the shared SSBO itself.
       This is independent of glUseProgram frequency, so a program that stays bound
       across multiple frames cannot keep a stale per-program frame uniform. */
    std::uint32_t nextSerial=static_cast<std::uint32_t>((frame_+1u)&0x7fffffffu);if(nextSerial==0u)nextSerial=1u;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,15*sizeof(std::uint32_t),sizeof(nextSerial),&nextSerial);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,(GLuint)oldBase0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER,(GLuint)oldGenericSsbo);
}

bool VoxelRuntime::readDiagnostics(VoxelDiagnostics& out) const{out={};if(!buffer_||eglGetCurrentContext()==EGL_NO_CONTEXT)return false;GLint old=0;glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&old);glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer_);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);auto* mapped=static_cast<const std::uint32_t*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,16*sizeof(std::uint32_t),GL_MAP_READ_BIT));if(!mapped){glBindBuffer(GL_SHADER_STORAGE_BUFFER,static_cast<GLuint>(old));return false;}std::array<std::uint32_t,16> h{};std::memcpy(h.data(),mapped,h.size()*sizeof(std::uint32_t));const bool unmapped=glUnmapBuffer(GL_SHADER_STORAGE_BUFFER)==GL_TRUE;glBindBuffer(GL_SHADER_STORAGE_BUFFER,static_cast<GLuint>(old));if(!unmapped)return false;auto asInt=[](std::uint32_t u){std::int32_t v{};std::memcpy(&v,&u,sizeof(v));return v;};out.valid=true;out.cameraX=asInt(h[0]);out.cameraY=asInt(h[1]);out.cameraZ=asInt(h[2]);out.cameraClaim=h[3];out.cameraCommit=h[4];out.captureFrame=h[5];out.sourceFrame=h[6];out.deferredFrame=h[7];out.giSampleFrame=h[8];out.computeSourceFrame=h[9];out.computeNonzeroFrame=h[10];out.computeFrame=h[11];out.drawFrameSerial=h[15];return true;}
} // namespace lvsgi
