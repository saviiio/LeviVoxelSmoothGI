#pragma once
#include <cstdint>
#include <string>
namespace lvsgi {
enum class PatchKind { None, DeferredLighting, ForwardWorldCapture, DisableNativeSsr };
struct PatchResult { std::string source; PatchKind kind{PatchKind::None}; int ssboBinding{-1}; bool changed{}; };
class ShaderPatcher { public: PatchResult patch(unsigned shaderType,const std::string& source,int maxBindings) const; static std::uint64_t fnv1a64(const std::string&); private: int chooseBinding(const std::string&,int maxBindings) const; };
} // namespace lvsgi
