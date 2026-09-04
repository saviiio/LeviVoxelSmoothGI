#pragma once
#include "lvsgi/Config.hpp"
#include <GLES3/gl31.h>
#include <cstdint>
#include <cstddef>

namespace lvsgi {
struct VoxelDiagnostics {
    bool valid{};
    std::int32_t cameraX{}, cameraY{}, cameraZ{};
    std::uint32_t cameraClaim{}, cameraCommit{};
    std::uint32_t captureFrame{}, sourceFrame{}, deferredFrame{}, giSampleFrame{};
    std::uint32_t computeSourceFrame{}, computeNonzeroFrame{}, computeFrame{}, drawFrameSerial{};
};

class VoxelRuntime {
public:
    bool ensure(const Config&);
    void shutdown();
    void endFrame(const Config&);
    void bindForProgram(int binding) const;
    GLuint buffer() const { return buffer_; }
    int parity() const { return parity_; }
    std::uint64_t frame() const { return frame_; }
    std::uint64_t dispatches() const { return dispatches_; }
    bool readDiagnostics(VoxelDiagnostics&) const;

private:
    bool compileCompute(const Config&);
    std::size_t uintCount(const Config&) const;
    GLuint buffer_{};
    GLuint compute_{};
    int parity_{};
    std::uint64_t frame_{};
    std::uint64_t dispatches_{};
    int dx_{}, dy_{}, dz_{};
    GLint uDims_{-1}, uParity_{-1}, uFrameSerial_{-1}, uDecay_{-1}, uTurn_{-1}, uBack_{-1};
};
} // namespace lvsgi
