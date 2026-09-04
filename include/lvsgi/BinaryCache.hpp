#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <deque>
#include <vector>
#include <atomic>
namespace lvsgi {
struct ProgramBinary { std::uint32_t format{}; std::vector<std::uint8_t> bytes; };
class BinaryCache {
public:
 BinaryCache()=default; ~BinaryCache();
 void start(std::filesystem::path root,std::size_t maxBytes,std::size_t maxEntries); void stop();
 std::shared_ptr<const ProgramBinary> find(const std::string& key) const;
 void storeAsync(std::string key,std::uint32_t format,std::vector<std::uint8_t> bytes);
 bool ready() const noexcept {return ready_.load(std::memory_order_acquire);}
private:
 void worker(); void preload(); void writeOne(const std::string&,const ProgramBinary&); void trim();
 std::filesystem::path root_; std::size_t maxBytes_{}; std::size_t maxEntries_{};
 mutable std::mutex mu_; std::condition_variable cv_; std::unordered_map<std::string,std::shared_ptr<ProgramBinary>> mem_; std::deque<std::pair<std::string,std::shared_ptr<ProgramBinary>>> writes_; std::thread thread_; bool stop_{}; std::atomic_bool ready_{false};
};
} // namespace lvsgi
