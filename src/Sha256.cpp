#include "lvsgi/Sha256.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>
namespace lvsgi { namespace { constexpr std::uint32_t K[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
inline std::uint32_t r(std::uint32_t x,int n){return (x>>n)|(x<<(32-n));}}
Sha256::Sha256():h_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}{}
void Sha256::update(const void* p0,std::size_t n){auto*p=(const std::uint8_t*)p0; bits_+=std::uint64_t(n)*8; while(n){auto m=std::min(n,64-used_); std::memcpy(buf_.data()+used_,p,m); used_+=m;p+=m;n-=m;if(used_==64){transform(buf_.data());used_=0;}}}
void Sha256::transform(const std::uint8_t*b){std::uint32_t w[64];for(int i=0;i<16;i++)w[i]=(std::uint32_t(b[i*4])<<24)|(std::uint32_t(b[i*4+1])<<16)|(std::uint32_t(b[i*4+2])<<8)|b[i*4+3];for(int i=16;i<64;i++){auto s0=r(w[i-15],7)^r(w[i-15],18)^(w[i-15]>>3);auto s1=r(w[i-2],17)^r(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}auto a=h_[0],c=h_[2],b0=h_[1],d=h_[3],e=h_[4],f=h_[5],g=h_[6],h=h_[7];for(int i=0;i<64;i++){auto S1=r(e,6)^r(e,11)^r(e,25);auto ch=(e&f)^((~e)&g);auto t1=h+S1+ch+K[i]+w[i];auto S0=r(a,2)^r(a,13)^r(a,22);auto maj=(a&b0)^(a&c)^(b0&c);auto t2=S0+maj;h=g;g=f;f=e;e=d+t1;d=c;c=b0;b0=a;a=t1+t2;}h_[0]+=a;h_[1]+=b0;h_[2]+=c;h_[3]+=d;h_[4]+=e;h_[5]+=f;h_[6]+=g;h_[7]+=h;}
std::array<std::uint8_t,32> Sha256::finish(){auto bits=bits_;buf_[used_++]=0x80;if(used_>56){while(used_<64)buf_[used_++]=0;transform(buf_.data());used_=0;}while(used_<56)buf_[used_++]=0;for(int i=7;i>=0;i--)buf_[used_++]=std::uint8_t(bits>>(i*8));transform(buf_.data());std::array<std::uint8_t,32>o{};for(int i=0;i<8;i++){o[i*4]=h_[i]>>24;o[i*4+1]=h_[i]>>16;o[i*4+2]=h_[i]>>8;o[i*4+3]=h_[i];}return o;}
std::string Sha256::hex(const std::array<std::uint8_t,32>&a){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:a)s<<std::setw(2)<<int(b);return s.str();}
} // namespace lvsgi
