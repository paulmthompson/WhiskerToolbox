/**
 * @file BatchLineStore.hpp
 * @brief GPU buffer owner for batch line segment data (SSBOs + CPU mirror)
 *
 * Owns the GPU-side Shader Storage Buffer Objects for batch line data and
 * maintains a CPU-side mirror (LineBatchData) so the CpuLineBatchIntersector
 * can query it without a GPU read-back.
 *
 * Both BatchLineRenderer and ComputeShaderIntersector reference the same
 * store — zero redundant buffer copies.
 *
 * Requires OpenGL 4.3+ for SSBO creation.  On platforms without 4.3
 * (macOS), the store can still hold CPU data for the CPU intersector,
 * but GPU buffers will not be created.
 */
#ifndef PLOTTINGOPENGL_LINEBATCH_BATCHLINESTORE_HPP
#define PLOTTINGOPENGL_LINEBATCH_BATCHLINESTORE_HPP

#include "GLSSBOBuffer.hpp"

#include "CorePlotting/LineBatch/LineBatchData.hpp"

#include <cstdint>
#include <vector>

namespace PlottingOpenGL {

/**
 * @brief SSBO binding points used by BatchLineStore.
 *
 * These must match the layout(std430, binding = N) declarations
 * in the compute and geometry shaders.
 */
namespace BatchLineBindings {
    inline constexpr std::uint32_t Segments           = 0;
    inline constexpr std::uint32_t IntersectionResults = 1;
    inline constexpr std::uint32_t IntersectionCount   = 2;
    inline constexpr std::uint32_t SelectionMask       = 3; ///< Shared by compute (binding 3) and geom shader
    inline constexpr std::uint32_t VisibilityMask      = 4; ///< Geom shader binding; compute uses binding 3 for vis
    // Note: the legacy compute shader used binding 3 for visibility.
    // The new architecture uses binding 4 for visibility in the geometry shader,
    // and binding 3 for visibility in the compute shader (matching the existing .comp).
    inline constexpr std::uint32_t VisibilityMaskCompute = 3; ///< Compute shader binding for visibility
} // namespace BatchLineBindings

class BatchLineStore {
public:
    BatchLineStore();
    ~BatchLineStore();

    BatchLineStore(BatchLineStore const &) = delete;
    BatchLineStore & operator=(BatchLineStore const &) = delete;

    /**
     * @brief Create GPU buffer objects.
     *
     * Call from initializeGL() with a current GL 4.3+ context.
     * Returns false if buffer creation fails or no context is available.
     * On macOS / GL < 4.3, returns false — CPU data is still usable.
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Release all GPU resources.
     *
     * Safe to call even if initialize() was never called or failed.
     */
    void cleanup();

    /// Whether GPU buffers have been successfully created.
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    // ── Full uploads ───────────────────────────────────────────────────

    /**
     * @brief Full re-upload of batch data to GPU (and update CPU mirror).
     *
     * This is the expensive path — called when the underlying data changes.
     * The segments are packed into the 5-float-per-segment format expected
     * by the compute shader (x1, y1, x2, y2, line_id_as_bits).
     */
    void upload(CorePlotting::LineBatchData const & batch);

    // ── Cheap partial updates ──────────────────────────────────────────

    /**
     * @brief Update only the visibility mask (CPU + GPU).
     *
     * Much cheaper than a full upload.  @p mask must have size == numLines().
     */
    void updateVisibilityMask(std::vector<std::uint32_t> const & mask);

    /**
     * @brief Update only the selection mask (CPU + GPU).
     *
     * @p mask must have size == numLines().
     */
    void updateSelectionMask(std::vector<std::uint32_t> const & mask);

    // ── Buffer accessors (for renderer and compute intersector) ────────

    [[nodiscard]] std::uint32_t segmentsBufferId() const { return m_segments_ssbo.bufferId(); }
    [[nodiscard]] std::uint32_t visibilityBufferId() const { return m_visibility_ssbo.bufferId(); }
    [[nodiscard]] std::uint32_t selectionMaskBufferId() const { return m_selection_ssbo.bufferId(); }
    [[nodiscard]] std::uint32_t intersectionResultsBufferId() const { return m_intersection_results_ssbo.bufferId(); }
    [[nodiscard]] std::uint32_t intersectionCountBufferId() const { return m_intersection_count_ssbo.bufferId(); }

    [[nodiscard]] std::uint32_t numSegments() const { return m_cpu_data.numSegments(); }
    [[nodiscard]] std::uint32_t numLines() const { return m_cpu_data.numLines(); }

    /// CPU-side data mirror (for CpuLineBatchIntersector or direct queries).
    [[nodiscard]] CorePlotting::LineBatchData const & cpuData() const { return m_cpu_data; }

    // ── Bind helpers ───────────────────────────────────────────────────

    /// Bind all SSBOs needed by the compute shader to their binding points.
    void bindForCompute() const;

    /// Bind all SSBOs needed by the rendering shader to their binding points.
    void bindForRender() const;

    /// Reset the intersection count atomic counter to zero (call before dispatch).
    void resetIntersectionCount();

    /// Maximum number of intersection results the results buffer can hold.
    static constexpr std::uint32_t RESULTS_CAPACITY = 100000;

private:
    CorePlotting::LineBatchData m_cpu_data;

    /// GPU segment data in compute-shader format: 5 floats per segment.
    std::vector<float> m_packed_segments;

    GLSSBOBuffer m_segments_ssbo;
    GLSSBOBuffer m_visibility_ssbo;
    GLSSBOBuffer m_selection_ssbo;
    GLSSBOBuffer m_intersection_results_ssbo;
    GLSSBOBuffer m_intersection_count_ssbo;

    /// Per-segment line IDs SSBO (used by the render path's line_id attribute
    /// when rendering via SSBOs instead of vertex attributes — future use).
    GLSSBOBuffer m_line_ids_ssbo;

    bool m_initialized{false};

    /// Pack LineBatchData into the 5-float-per-segment format for the compute shader.
    void packSegments();
};

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_LINEBATCH_BATCHLINESTORE_HPP
