#pragma once
#include "lvsgi/Config.hpp"
#include <pl/Config.hpp>
#include <string_view>

namespace pl::config {

template <> struct Schema<lvsgi::Config> {
    static constexpr std::string_view title = "Levi Voxel Smooth GI";
    static constexpr std::string_view description =
        "RenderDragon shader cache, one-block hierarchical DDA reflections and directional flood-fill GI.";

private:
    // FieldSchema is an aggregate in preloader-android 0.2.2. Avoid partial
    // designated initializers here: Android NDK clang diagnoses every omitted
    // member with -Wmissing-designated-field-initializers, and this project
    // intentionally builds with -Werror.
    static constexpr FieldSchema textField(
        std::string_view fieldTitle,
        std::string_view fieldDescription,
        bool readOnly = false) {
        FieldSchema schema{};
        schema.title = fieldTitle;
        schema.description = fieldDescription;
        schema.readOnly = readOnly;
        return schema;
    }

    static constexpr FieldSchema rangedField(double minimum, double maximum) {
        FieldSchema schema{};
        schema.minimum = minimum;
        schema.maximum = maximum;
        return schema;
    }

public:
    static constexpr FieldSchema field(std::string_view name) {
        if (name == "version")
            return textField("Version", "Configuration format version.", true);
        if (name == "enabled")
            return textField("Enabled", "Enable the native mod.");
        if (name == "voxelX" || name == "voxelZ") return rangedField(24.0, 128.0);
        if (name == "voxelY") return rangedField(16.0, 96.0);
        if (name == "reflectionRange") return rangedField(4.0, 128.0);
        if (name == "reflectionStrength") return rangedField(0.0, 2.0);
        if (name == "giStrength") return rangedField(0.0, 3.0);
        if (name == "giDecay") return rangedField(0.5, 0.99);
        if (name == "giTurn") return rangedField(0.0, 0.25);
        if (name == "giBackTurn") return rangedField(0.0, 0.1);
        if (name == "maxCacheMiB") return rangedField(16.0, 1024.0);
        if (name == "maxCacheEntries") return rangedField(128.0, 32768.0);
        if (name == "binaryCaptureDelayFrames") return rangedField(0.0, 2000.0);
        if (name == "binaryCaptureIntervalFrames") return rangedField(1.0, 120.0);
        if (name == "stallLogMs") return rangedField(1.0, 100.0);
        if (name == "compilerThreads") return rangedField(1.0, 16.0);
        if (name == "prewarmStartDelayFrames") return rangedField(0.0, 1200.0);
        if (name == "prewarmProgramsPerFrame") return rangedField(0.0, 8.0);
        if (name == "prewarmMaxPending") return rangedField(1.0, 64.0);
        if (name == "minecraftHookWarningMs") return rangedField(1000.0, 120000.0);
        return {};
    }
};

} // namespace pl::config
