#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace lvsgi {
class Sha256 { public: Sha256(); void update(const void*,std::size_t); void update(std::string_view s){update(s.data(),s.size());} std::array<std::uint8_t,32> finish(); static std::string hex(const std::array<std::uint8_t,32>&); private: void transform(const std::uint8_t*); std::array<std::uint32_t,8> h_; std::array<std::uint8_t,64> buf_{}; std::uint64_t bits_{}; std::size_t used_{}; };
} // namespace lvsgi
