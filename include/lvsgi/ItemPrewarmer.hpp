#pragma once
#include <GLES3/gl31.h>
#include <filesystem>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
namespace lvsgi {
class ItemPrewarmer {
public:
 ~ItemPrewarmer();
 void start(const std::filesystem::path& file);
 void stop();
 void tick(std::uint64_t frame,int startDelay,int programsPerFrame,int maxPending,bool parallelCompile);
 bool ready() const;
private:
 struct Shader { GLenum stage{}; std::string source; };
 struct Pair { std::uint32_t vertex{},fragment{}; };
 void load(const std::filesystem::path&);
 GLuint launch(const Pair&);
 mutable std::mutex mu_;
 std::thread loader_;
 std::vector<Shader> shaders_;
 std::vector<Pair> pairs_;
 std::vector<GLuint> pending_;
 std::size_t next_{};
 bool ready_{},stop_{},doneLogged_{};
};
} // namespace lvsgi
