#pragma once
#include <filesystem>
#include <string>
#include <string_view>
namespace pl::log { class Logger { public: template<class...A> void info(std::string_view,A&&...) const{} template<class...A> void warn(std::string_view,A&&...) const{} template<class...A> void error(std::string_view,A&&...) const{} }; }
namespace ll::mod { class NativeMod { public: static NativeMod* current(){static NativeMod x;return &x;} pl::log::Logger& getLogger(){static pl::log::Logger l;return l;} std::filesystem::path getModDir()const{return{};} std::filesystem::path getDataDir()const{return{};} std::filesystem::path getConfigDir()const{return{};} std::filesystem::path getResourceDir()const{return{};} }; }
#define PL_REGISTER_MOD(T,E)
