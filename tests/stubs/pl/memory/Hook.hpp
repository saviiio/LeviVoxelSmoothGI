#pragma once
namespace pl::memory {
enum class HookPriority:int{Highest=0,High=100,Normal=200,Low=300,Lowest=400};
class HookHandle {
public:
 HookHandle()=default;
 HookHandle(void*,void*,void**,HookPriority=HookPriority::Normal){}
 HookHandle(HookHandle&&)=default; HookHandle& operator=(HookHandle&&)=default;
 HookHandle(const HookHandle&)=delete; HookHandle& operator=(const HookHandle&)=delete;
 bool installed() const{return true;} void reset(){}
};
}
