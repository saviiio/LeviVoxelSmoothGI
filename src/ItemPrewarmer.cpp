#include "lvsgi/ItemPrewarmer.hpp"
#include "lvsgi/Log.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>
#ifndef GL_COMPLETION_STATUS_KHR
#define GL_COMPLETION_STATUS_KHR 0x91B1
#endif
namespace lvsgi {
namespace { template<class T> bool readT(std::ifstream&f,T&v){return bool(f.read(reinterpret_cast<char*>(&v),sizeof(v)));} }
ItemPrewarmer::~ItemPrewarmer(){stop();}
bool ItemPrewarmer::ready()const{std::lock_guard l(mu_);return ready_;}
void ItemPrewarmer::start(const std::filesystem::path& file){stop();{std::lock_guard l(mu_);stop_=false;ready_=false;doneLogged_=false;next_=0;}loader_=std::thread([this,file]{load(file);});}
void ItemPrewarmer::stop(){if(loader_.joinable())loader_.join();std::lock_guard l(mu_);stop_=true;pending_.clear();shaders_.clear();pairs_.clear();ready_=false;next_=0;}
void ItemPrewarmer::load(const std::filesystem::path& file){std::ifstream f(file,std::ios::binary);char magic[8]{};std::uint32_t ns=0,np=0;if(!f.read(magic,8)||std::memcmp(magic,"LVPR1\0\0\0",8)!=0||!readT(f,ns)||!readT(f,np)||ns>4096||np>8192){LVSGI_W("item prewarm database unavailable or invalid");std::lock_guard l(mu_);ready_=true;return;}std::vector<Shader> sh;std::vector<Pair> pa;sh.reserve(ns);pa.reserve(np);for(std::uint32_t i=0;i<ns;i++){std::uint8_t type=0;std::uint32_t len=0;if(!readT(f,type)||!readT(f,len)||len>1024u*1024u)return;Shader s;s.stage=type==0?GL_VERTEX_SHADER:GL_FRAGMENT_SHADER;s.source.resize(len);if(!f.read(s.source.data(),len))return;sh.push_back(std::move(s));}for(std::uint32_t i=0;i<np;i++){Pair p;if(!readT(f,p.vertex)||!readT(f,p.fragment)||p.vertex>=ns||p.fragment>=ns)return;pa.push_back(p);}std::lock_guard l(mu_);if(stop_)return;shaders_=std::move(sh);pairs_=std::move(pa);ready_=true;LVSGI_I("item pipeline prewarm database loaded: %zu shaders / %zu programs",shaders_.size(),pairs_.size());}
GLuint ItemPrewarmer::launch(const Pair&p){if(p.vertex>=shaders_.size()||p.fragment>=shaders_.size())return 0;auto make=[&](const Shader&s){GLuint id=glCreateShader(s.stage);const GLchar*src=s.source.c_str();GLint len=(GLint)s.source.size();glShaderSource(id,1,&src,&len);glCompileShader(id);return id;};GLuint vs=make(shaders_[p.vertex]),fs=make(shaders_[p.fragment]);if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}GLuint pr=glCreateProgram();glAttachShader(pr,vs);glAttachShader(pr,fs);glLinkProgram(pr);glDeleteShader(vs);glDeleteShader(fs);return pr;}
void ItemPrewarmer::tick(std::uint64_t frame,int startDelay,int programsPerFrame,int maxPending,bool parallelCompile){std::lock_guard l(mu_);if(!ready_||stop_||frame<(std::uint64_t)std::max(0,startDelay))return;if(parallelCompile){for(auto it=pending_.begin();it!=pending_.end();){GLint done=0;glGetProgramiv(*it,GL_COMPLETION_STATUS_KHR,&done);if(done==GL_TRUE){glDeleteProgram(*it);it=pending_.erase(it);}else ++it;}}else{for(GLuint p:pending_)glDeleteProgram(p);pending_.clear();}
 int launched=0;int count=std::max(0,programsPerFrame);while(next_<pairs_.size()&&launched<count&&(!parallelCompile||(int)pending_.size()<std::max(1,maxPending))){GLuint p=launch(pairs_[next_++]);if(p){if(parallelCompile)pending_.push_back(p);else{GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);glDeleteProgram(p);}}launched++;}
 if(next_>=pairs_.size()&&pending_.empty()&&!doneLogged_){doneLogged_=true;LVSGI_I("item pipeline prewarm complete: %zu programs",pairs_.size());}}
} // namespace lvsgi
