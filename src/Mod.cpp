#include "lvsgi/Config.hpp"
#include "lvsgi/ConfigSchema.hpp"
#include "lvsgi/GlHooks.hpp"

#include <pl/Mod.hpp>
#include <pl/Config.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <exception>

namespace {

class LeviVoxelSmoothGIMod {
public:
    static LeviVoxelSmoothGIMod& instance() {
        static LeviVoxelSmoothGIMod mod;
        return mod;
    }

    LeviVoxelSmoothGIMod() : mSelf_(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf_; }

    bool load() {
        try {
            auto& self = getSelf();
            std::filesystem::create_directories(self.getConfigDir());
            std::filesystem::create_directories(self.getDataDir());

            config_.emplace(lvsgi::Config{});
            if (!config_->load()) {
                self.getLogger().error("Failed to load/normalize typed config");
                return false;
            }
            cfg_ = config_->value();
            lvsgi::sanitizeConfig(cfg_);
            config_->value() = cfg_;
            if (!config_->save())
                self.getLogger().warn("Could not persist sanitized config values");
            hooks_ = std::make_unique<lvsgi::GlHooks>();

            self.getLogger().info(
                "LeviVoxelSmoothGI beta.8.6 loaded; modDir={} dataDir={} configDir={}",
                self.getModDir().string(), self.getDataDir().string(),
                self.getConfigDir().string());
            return true;
        } catch (const std::exception& e) {
            getSelf().getLogger().error("load() failed: {}", e.what());
            return false;
        } catch (...) {
            getSelf().getLogger().error("load() failed with unknown exception");
            return false;
        }
    }

    bool enable() {
        try {
            auto& self = getSelf();
            if (!config_) {
                self.getLogger().error("enable() called without loaded ConfigFile");
                return false;
            }
            cfg_ = config_->value();
            lvsgi::sanitizeConfig(cfg_);
            if (!cfg_.enabled) {
                self.getLogger().info("LeviVoxelSmoothGI is disabled by config");
                return true;
            }
            if (!hooks_) hooks_ = std::make_unique<lvsgi::GlHooks>();

            const auto prewarm = self.getResourceDir() / "current_item_programs.lvpr";
            const bool ok = hooks_->arm(cfg_, self.getDataDir(), prewarm);
            if (ok) {
                self.getLogger().info(
                    "MOD ENABLED: waiting for libminecraftpe.so; version-independent ELF relocation + AArch64 PLT discovery with Patch API validation is the primary path");
            } else {
                self.getLogger().error("Failed to arm LeviVoxelSmoothGI runtime");
            }
            return ok;
        } catch (const std::exception& e) {
            getSelf().getLogger().error("enable() failed: {}", e.what());
            return false;
        } catch (...) {
            getSelf().getLogger().error("enable() failed with unknown exception");
            return false;
        }
    }

    bool disable() {
        try {
            if (hooks_) hooks_->uninstall();
            getSelf().getLogger().info("LeviVoxelSmoothGI disabled; HookHandles reset");
            return true;
        } catch (const std::exception& e) {
            getSelf().getLogger().error("disable() failed: {}", e.what());
            return false;
        } catch (...) {
            getSelf().getLogger().error("disable() failed with unknown exception");
            return false;
        }
    }

    bool unload() {
        try {
            if (hooks_) hooks_->uninstall();
            hooks_.reset();
            config_.reset();
            getSelf().getLogger().info("LeviVoxelSmoothGI unloaded");
            return true;
        } catch (const std::exception& e) {
            getSelf().getLogger().error("unload() failed: {}", e.what());
            return false;
        } catch (...) {
            getSelf().getLogger().error("unload() failed with unknown exception");
            return false;
        }
    }

private:
    ll::mod::NativeMod& mSelf_;
    lvsgi::Config cfg_{};
    std::optional<pl::config::ConfigFile<lvsgi::Config>> config_;
    std::unique_ptr<lvsgi::GlHooks> hooks_;
};

} // namespace

PL_REGISTER_MOD(LeviVoxelSmoothGIMod, LeviVoxelSmoothGIMod::instance())
