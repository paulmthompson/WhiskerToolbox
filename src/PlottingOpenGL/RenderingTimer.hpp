#ifndef PLOTTINGOPENGL_RENDERINGTIMER_HPP
#define PLOTTINGOPENGL_RENDERINGTIMER_HPP

/**
 * @file RenderingTimer.hpp
 * @brief RAII-based timing utilities for measuring rendering performance
 * 
 * Provides simple timing instrumentation that can be enabled/disabled
 * at runtime. Designed for profiling the rendering pipeline to identify
 * bottlenecks.
 * 
 * Usage:
 * @code
 *   RenderingTimerStats stats;
 *   
 *   {
 *       ScopedTimer timer(stats.batch_building_time);
 *       // ... batch building code ...
 *   }
 *   
 *   {
 *       ScopedTimer timer(stats.gpu_upload_time);
 *       // ... GPU upload code ...
 *   }
 *   
 *   std::cout << "Batch building: " << stats.batch_building_time.count() << "µs\n";
 *   std::cout << "GPU upload: " << stats.gpu_upload_time.count() << "µs\n";
 * @endcode
 */

#include <chrono>
#include <atomic>
#include <string>

namespace PlottingOpenGL {

/**
 * @brief Accumulated timing statistics for rendering operations
 */
struct RenderingTimerStats {
    /// Time spent building CPU-side batches (vertex generation)
    std::chrono::microseconds batch_building_time{0};
    
    /// Time spent uploading data to GPU
    std::chrono::microseconds gpu_upload_time{0};
    
    /// Time spent in actual draw calls
    std::chrono::microseconds draw_call_time{0};
    
    /// Total frame time
    std::chrono::microseconds total_frame_time{0};
    
    /// Number of frames measured
    int frame_count{0};
    
    /// Number of vertices uploaded this frame
    size_t vertices_uploaded{0};
    
    /// Number of bytes uploaded this frame
    size_t bytes_uploaded{0};
    
    void reset() {
        batch_building_time = std::chrono::microseconds{0};
        gpu_upload_time = std::chrono::microseconds{0};
        draw_call_time = std::chrono::microseconds{0};
        total_frame_time = std::chrono::microseconds{0};
        frame_count = 0;
        vertices_uploaded = 0;
        bytes_uploaded = 0;
    }
    
    /// Get average batch building time per frame
    [[nodiscard]] double avgBatchBuildingMs() const {
        if (frame_count == 0) return 0.0;
        return static_cast<double>(batch_building_time.count()) / static_cast<double>(frame_count) / 1000.0;
    }
    
    /// Get average GPU upload time per frame
    [[nodiscard]] double avgGpuUploadMs() const {
        if (frame_count == 0) return 0.0;
        return static_cast<double>(gpu_upload_time.count()) / static_cast<double>(frame_count) / 1000.0;
    }
    
    /// Get average draw call time per frame
    [[nodiscard]] double avgDrawCallMs() const {
        if (frame_count == 0) return 0.0;
        return static_cast<double>(draw_call_time.count()) / static_cast<double>(frame_count) / 1000.0;
    }
    
    /// Get average total frame time
    [[nodiscard]] double avgTotalMs() const {
        if (frame_count == 0) return 0.0;
        return static_cast<double>(total_frame_time.count()) / static_cast<double>(frame_count) / 1000.0;
    }
    
    /// Get summary string
    [[nodiscard]] std::string summary() const {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
            "Frames: %d | Batch: %.2fms | Upload: %.2fms | Draw: %.2fms | Total: %.2fms | Vertices: %zu",
            frame_count, avgBatchBuildingMs(), avgGpuUploadMs(), avgDrawCallMs(), avgTotalMs(),
            frame_count > 0 ? vertices_uploaded / static_cast<size_t>(frame_count) : 0);
        return std::string(buffer);
    }
};

/**
 * @brief Global flag to enable/disable timing (minimizes overhead when disabled)
 */
inline std::atomic<bool> g_timing_enabled{false};

/**
 * @brief RAII timer that adds elapsed time to a duration
 */
class ScopedTimer {
public:
    explicit ScopedTimer(std::chrono::microseconds& target, bool enabled = true)
        : m_target(target), m_enabled(enabled && g_timing_enabled.load(std::memory_order_relaxed)) {
        if (m_enabled) {
            m_start = std::chrono::high_resolution_clock::now();
        }
    }
    
    ~ScopedTimer() {
        if (m_enabled) {
            auto const end = std::chrono::high_resolution_clock::now();
            m_target += std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
        }
    }
    
    // Non-copyable, non-movable
    ScopedTimer(ScopedTimer const&) = delete;
    ScopedTimer& operator=(ScopedTimer const&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    std::chrono::microseconds& m_target;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    bool m_enabled;
};

/**
 * @brief Enable or disable global timing
 */
inline void setTimingEnabled(bool enabled) {
    g_timing_enabled.store(enabled, std::memory_order_relaxed);
}

/**
 * @brief Check if timing is enabled
 */
inline bool isTimingEnabled() {
    return g_timing_enabled.load(std::memory_order_relaxed);
}

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERINGTIMER_HPP
