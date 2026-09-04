#pragma once
#include <filesystem>
#include <optional>
#include <string_view>
namespace pl::config {
struct FieldSchema { std::string_view title{}; std::string_view description{}; std::optional<double> minimum{}; std::optional<double> maximum{}; bool readOnly=false; };
template<class T> struct Schema { static constexpr std::string_view title={}; static constexpr std::string_view description={}; static constexpr FieldSchema field(std::string_view){return{};} };
template<class T> class ConfigFile { T v_{}; public: explicit ConfigFile(T d=T{}):v_(d){} bool load(){return true;} bool save()const{return true;} T& value(){return v_;} const T& value()const{return v_;} };
}
