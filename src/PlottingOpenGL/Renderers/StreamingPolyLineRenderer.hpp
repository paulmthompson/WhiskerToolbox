#ifndef PLOTTINGOPENGL_RENDERERS_STREAMINGPOLYLINERENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_STREAMINGPOLYLINERENDERER_HPP

/**
 * @file StreamingPolyLineRenderer.hpp
 * @brief Optimized polyline renderer for scrolling time series data.
 * 
 * This renderer is designed for scenarios where time series data scrolls
 * horizontally and most of the data remains unchanged between frames.
 * Instead of re-uploading the entire buffer each frame, it uses a ring
 * buffer strategy with glBufferSubData to update only the changed portions.
 * 
 * ## Optimization Strategy
 * 
 * When scrolling time series data, typically:
 * - Leading edge: new data points enter the view
 * - Trailing edge: old data points exit the view  
 * - Middle: most data points remain unchanged
 * 
 * This renderer maintains:
 * 1. A GPU buffer with extra capacity (e.g., 2x-3x visible points)
 * 2. Cached vertex data on CPU for comparison
 * 3. Dirty region tracking to minimize glBufferSubData calls
 * 
 * ## Usage Pattern
 * 
 * @code
 *   StreamingPolyLineRenderer renderer;
 *   renderer.initialize();
 *   
 *   // On each frame:
 *   renderer.updateData(batch);  // Incremental update
 *   renderer.render(view, projection);
 *   
 *   // Timing:
 *   auto stats = renderer.getTimingStats();
 * @endcode
 * 
 * @see PolyLineRenderer for the non-optimized version
 */

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace PlottingOpenGL {

/**
 * @brief Timing statistics for performance analysis
 */
struct RenderTimingStats {
    std::chrono::microseconds last_upload_time{0};
    std::chrono::microseconds last_render_time{0};
    std::chrono::microseconds last_total_time{0};
    
    // Upload breakdown
    size_t bytes_uploaded{0};
    size_t bytes_total{0};
    bool was_full_reupload{false};
    
    // Moving averages (updated internally)
    double avg_upload_time_us{0.0};
    double avg_render_time_us{0.0};
    int sample_count{0};
    
    void reset() {
        last_upload_time = std::chrono::microseconds{0};
        last_render_time = std::chrono::microseconds{0};
        last_total_time = std::chrono::microseconds{0};
        bytes_uploaded = 0;
        bytes_total = 0;
        was_full_reupload = false;
        avg_upload_time_us = 0.0;
        avg_render_time_us = 0.0;
        sample_count = 0;
    }
};

/**
 * @brief Optimized polyline renderer with incremental GPU buffer updates.
 * 
 * Uses glBufferSubData to update only changed portions of the vertex buffer
 * when time series data scrolls.
 */
class StreamingPolyLineRenderer : public IBatchRenderer {
public:
    /**
     * @brief Construct a StreamingPolyLineRenderer
     * @param shader_base_path Base path to shader directory (empty = embedded)
     * @param capacity_multiplier How much extra buffer capacity to allocate (default: 2.0x)
     */
    explicit StreamingPolyLineRenderer(std::string shader_base_path = "",
                                       float capacity_multiplier = 2.0f);
    ~StreamingPolyLineRenderer() override;

    // IBatchRenderer interface
    [[nodiscard]] bool initialize() override;
    void cleanup() override;
    [[nodiscard]] bool isInitialized() const override;
    void render(glm::mat4 const & view_matrix,
                glm::mat4 const & projection_matrix) override;
    [[nodiscard]] bool hasData() const override;
    void clearData() override;

    /**
     * @brief Update buffer with new batch data using incremental strategy
     * 
     * Compares new data with cached data and uses glBufferSubData
     * to update only the changed regions when possible. Falls back
     * to full re-upload when:
     * - Buffer capacity is insufficient
     * - Topology changes (number of lines, line lengths)
     * - First upload after clearData()
     * 
     * @param batch The new polyline batch
     */
    void updateData(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Force a full re-upload of the buffer (bypasses incremental logic)
     * 
     * Use this when you know the data has changed significantly,
     * such as when loading a new dataset.
     */
    void uploadData(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Enable/disable timing measurement
     * 
     * When enabled, timing data is collected for each updateData/render call.
     * Disable in production for slightly better performance.
     */
    void setTimingEnabled(bool enabled) { m_timing_enabled = enabled; }
    [[nodiscard]] bool isTimingEnabled() const { return m_timing_enabled; }

    /**
     * @brief Get timing statistics from the last frame
     */
    [[nodiscard]] RenderTimingStats const & getTimingStats() const { return m_timing_stats; }

    /**
     * @brief Reset timing statistics (clears moving averages)
     */
    void resetTimingStats() { m_timing_stats.reset(); }

    /**
     * @brief Get cache hit ratio (0.0 = all misses, 1.0 = all hits)
     * 
     * A "hit" means incremental update was possible.
     * A "miss" means full re-upload was required.
     */
    [[nodiscard]] float getCacheHitRatio() const;

    /**
     * @brief Set the tolerance for comparing vertices (default: 0.0)
     * 
     * Vertices within this tolerance are considered equal.
     * Useful when floating point precision causes minor variations.
     */
    void setComparisonTolerance(float tolerance) { m_comparison_tolerance = tolerance; }

private:
    bool loadShadersFromManager();
    bool compileEmbeddedShaders();
    void setupVertexAttributes();

    /**
     * @brief Compare new batch with cached data and compute dirty regions
     * @return true if incremental update is possible, false if full re-upload needed
     */
    bool computeDirtyRegions(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Perform incremental GPU buffer update using glBufferSubData
     */
    void updateGPUBufferIncremental();

    /**
     * @brief Perform full GPU buffer allocation and upload
     */
    void uploadGPUBufferFull(CorePlotting::RenderablePolyLineBatch const & batch);

    std::string m_shader_base_path;
    float m_capacity_multiplier{2.0f};
    bool m_use_shader_manager{false};

    GLShaderProgram m_embedded_shader;
    GLVertexArray m_vao;
    GLBuffer m_vbo{GLBuffer::Type::Vertex};

    // GPU buffer state
    size_t m_gpu_buffer_capacity{0};     // Allocated capacity in bytes
    size_t m_gpu_buffer_used{0};         // Used portion in bytes

    // Cached batch data (CPU copy for comparison)
    struct CachedBatchData {
        std::vector<float> vertices;
        std::vector<int32_t> line_start_indices;
        std::vector<int32_t> line_vertex_counts;
        glm::vec4 global_color{1.0f, 1.0f, 1.0f, 1.0f};
        glm::mat4 model_matrix{1.0f};
        float thickness{1.0f};
        bool valid{false};
    };
    CachedBatchData m_cached_batch;

    // Dirty region tracking
    struct DirtyRegion {
        size_t start_byte{0};
        size_t end_byte{0};
    };
    std::vector<DirtyRegion> m_dirty_regions;
    std::vector<float> m_pending_vertices;  // New vertex data to upload

    // Timing and statistics
    bool m_timing_enabled{false};
    RenderTimingStats m_timing_stats;
    int m_total_updates{0};
    int m_incremental_updates{0};

    float m_comparison_tolerance{0.0f};
    bool m_initialized{false};

    static constexpr char const * SHADER_PROGRAM_NAME = "streaming_polyline_renderer";
};

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_STREAMINGPOLYLINERENDERER_HPP
