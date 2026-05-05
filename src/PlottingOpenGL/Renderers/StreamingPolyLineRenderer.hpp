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
     *
     * @pre initialize() must have been called and returned true before the first call (enforcement: runtime_check)
     * @pre A valid OpenGL context must be current on the calling thread (enforcement: none) [CRITICAL]
     * @pre batch.line_start_indices.size() == batch.line_vertex_counts.size() (topology arrays must be parallel) (enforcement: none) [IMPORTANT]
     * @pre For each i: batch.line_start_indices[i] + batch.line_vertex_counts[i] <= batch.vertices.size() / 2 (no index exceeds the vertex buffer) (enforcement: none) [CRITICAL]
     * @pre batch.thickness > 0 (required by glLineWidth; zero or negative produces a GL error) (enforcement: none) [LOW]
     */
    void updateData(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Force a full re-upload of the buffer (bypasses incremental logic)
     * 
     * Use this when you know the data has changed significantly,
     * such as when loading a new dataset.
     *
     * An empty `batch.vertices` is handled gracefully: the buffer is cleared
     * and the function returns without touching the GPU.
     *
     * @pre initialize() must have been called and returned true before the first call (enforcement: runtime_check)
     * @pre A valid OpenGL context must be current on the calling thread (enforcement: none) [CRITICAL]
     * @pre batch.line_start_indices.size() == batch.line_vertex_counts.size() (topology arrays must be parallel) (enforcement: none) [IMPORTANT]
     * @pre For each i: batch.line_start_indices[i] + batch.line_vertex_counts[i] <= batch.vertices.size() / 2 (no index exceeds the vertex buffer; checked against the cached batch used by render()) (enforcement: none) [CRITICAL]
     * @pre batch.thickness > 0 (required by glLineWidth in render(); zero or negative produces a GL error) (enforcement: none) [LOW]
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
     * @brief Set the tolerance for comparing vertices 
     * 
     * Default: 0.0
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
     *
     * Populates `m_dirty_regions` and `m_pending_vertices` as a side effect.
     * Returns `false` (full re-upload) whenever topology changes, the vertex count
     * changes, the GPU buffer has insufficient capacity, or more than 50% of floats
     * differ. All of these conditions are runtime-checked internally.
     *
     * @return true if incremental update is possible, false if full re-upload needed
     *
     * @pre Called only from updateData(), which has already checked m_initialized
     *      (enforcement: runtime_check — via caller guard, not re-checked here)
     * @pre batch.line_start_indices.size() == batch.line_vertex_counts.size()
     *      (topology arrays must be parallel; mismatches are not detected here —
     *      they propagate silently into m_cached_batch and are consumed by render())
     *      (enforcement: none) [IMPORTANT]
     * @pre m_comparison_tolerance >= 0.0f (negative values cause every vertex float
     *      to compare as dirty, collapsing all incremental updates into full re-uploads)
     *      (enforcement: none) [LOW]
     * @pre m_comparison_tolerance is not NaN (NaN comparisons are always false, so no
     *      vertex is ever marked dirty, causing updateGPUBufferIncremental to skip all
     *      uploads and leave the GPU buffer silently stale)
     *      (enforcement: none) [IMPORTANT]
     *
     * @post If returns true: m_dirty_regions and m_pending_vertices are populated for
     *       updateGPUBufferIncremental(). If returns false: m_dirty_regions is empty
     *       and m_pending_vertices holds a copy of batch.vertices.
     */
    bool computeDirtyRegions(CorePlotting::RenderablePolyLineBatch const & batch);

    /**
     * @brief Perform incremental GPU buffer update using glBufferSubData
     *
     * Uploads only the byte ranges recorded in `m_dirty_regions` using
     * `glBufferSubData`, then moves `m_pending_vertices` into the CPU cache.
     * Returns early (no-op) if `GLFunctions::get()` is null or `m_dirty_regions`
     * is empty.
     *
     * @pre computeDirtyRegions() must have been called and returned true in the
     *      same frame before this is called (enforcement: none) [CRITICAL]
     *      — this is the only way m_dirty_regions and m_pending_vertices can
     *      carry the invariants this method relies on:
     *        • Every region has end_byte >= start_byte
     *        • Every region's start_byte is a multiple of sizeof(float)
     *        • Every region's end_byte <= m_pending_vertices.size() * sizeof(float)
     *        • Every region's end_byte <= m_gpu_buffer_capacity
     *      Violating any of these causes size_t underflow, misaligned VBO reads,
     *      out-of-bounds pointer arithmetic, or writes past the allocated GPU buffer.
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — GLFunctions::get() non-null is checked, but does not guarantee a
     *      current context; m_vao.bind() and m_vbo.write() will silently corrupt
     *      state or crash without one.
     * @pre m_initialized == true — enforced by the caller chain via updateData()
     *      (enforcement: runtime_check — via caller guard, not re-checked here)
     *
     * @post m_dirty_regions is cleared. If any upload was performed,
     *       m_cached_batch.vertices == (the vertices from the batch passed to
     *       computeDirtyRegions()) and m_cached_batch.valid == true.
     */
    void updateGPUBufferIncremental();

    /**
     * @brief Perform full GPU buffer allocation and upload
     *
     * Allocates (or reuses) a GPU buffer of `batch.vertices.size() * sizeof(float)
     * * m_capacity_multiplier` bytes, then uploads all vertex data via `glBufferData`
     * / `glBufferSubData`. An empty `batch.vertices` is handled gracefully by calling
     * `clearData()` and returning early.
     *
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — m_vao.bind() and m_vbo.write() require one; no context guard exists here.
     * @pre m_capacity_multiplier > 0.0f and not NaN (enforcement: none) [CRITICAL]
     *      — zero: desired_capacity computes as 0, allocating a 0-byte buffer, then
     *        m_vbo.write() immediately writes required_bytes past the end (GPU corruption).
     *      — negative or NaN: static_cast<size_t>(negative/NaN float) is C++ undefined
     *        behaviour (float-to-unsigned conversion of out-of-range value).
     *      In practice, m_capacity_multiplier defaults to 2.0f and is only set via the
     *      constructor, so this risk only surfaces with an explicit bad constructor argument.
     * @pre m_initialized == true — enforced by the caller chain via updateData() /
     *      uploadData() (enforcement: runtime_check — via caller guards, not re-checked here)
     * @pre batch.line_start_indices.size() == batch.line_vertex_counts.size()
     *      (topology arrays must be parallel — both are written to m_cached_batch and
     *      consumed by render() without further validation) (enforcement: none) [IMPORTANT]
     * @pre For each i: batch.line_start_indices[i] + batch.line_vertex_counts[i]
     *      <= batch.vertices.size() / 2 (no index exceeds the uploaded vertex buffer)
     *      (enforcement: none) [CRITICAL]
     * @pre batch.thickness > 0 (required by glLineWidth in render()) (enforcement: none) [LOW]
     * @pre batch.vertices.size() * sizeof(float) <= INT_MAX (narrowing cast to int for
     *      m_vbo.write() and m_vbo.allocate()) (enforcement: none) [LOW]
     *
     * @post m_cached_batch mirrors batch exactly and m_cached_batch.valid == true.
     *       m_gpu_buffer_capacity >= batch.vertices.size() * sizeof(float).
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

/**
 * @brief Embedded fallback shader source code for the polyline renderer.
 * 
 * These match the interface of WhiskerToolbox/shaders/line.vert and line.frag
 * but are embedded for cases where shader files are not available.
 */
namespace StreamingPolyLineShaders {


constexpr char const * VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec2 a_position;

uniform mat4 u_mvp_matrix;

void main() {
    gl_Position = u_mvp_matrix * vec4(a_position, 0.0, 1.0);
}
)";

constexpr char const * GEOMETRY_SHADER = R"(
#version 410 core

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float u_line_width;
uniform vec2 u_viewport_size;

void main() {
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;

    vec2 sp0 = p0 * u_viewport_size * 0.5;
    vec2 sp1 = p1 * u_viewport_size * 0.5;

    vec2 dir = sp1 - sp0;
    float len = length(dir);
    if (len < 0.001) return;
    dir /= len;
    vec2 perp = vec2(-dir.y, dir.x);

    vec2 offset_ndc = perp * u_line_width / u_viewport_size;

    gl_Position = vec4(p0 + offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p0 - offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p1 + offset_ndc, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p1 - offset_ndc, 0.0, 1.0);
    EmitVertex();
    EndPrimitive();
}
)";

constexpr char const * FRAGMENT_SHADER = R"(
#version 410 core

uniform vec4 u_color;

out vec4 FragColor;

void main() {
    FragColor = u_color;
}
)";

}// namespace StreamingPolyLineShaders

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_STREAMINGPOLYLINERENDERER_HPP
