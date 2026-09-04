#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace pl::memory {
inline std::vector<std::uint8_t> readBytes(std::uintptr_t, std::size_t) { return {}; }
}
