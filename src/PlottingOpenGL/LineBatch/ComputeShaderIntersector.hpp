/**
 * @file ComputeShaderIntersector.hpp
 * @brief OpenGL 4.3 compute shader implementation of ILineBatchIntersector
 *
 * Dispatches the line_intersection.comp shader to find all lines in a
 * BatchLineStore whose segments intersect a query line.  Reads segment data
 * from the store's shared SSBOs — no redundant buffer copies.
 *
 * Falls back to CpuLineBatchIntersector at runtime on systems without
 * compute shader support (see BatchLineStore::isInitialized()).
 */
#ifndef PLOTTINGOPENGL_LINEBATCH_COMPUTESHADERINTERSECTOR_HPP
#define PLOTTINGOPENGL_LINEBATCH_COMPUTESHADERINTERSECTOR_HPP

#include "BatchLineStore.hpp"

#include "CorePlotting/LineBatch/ILineBatchIntersector.hpp"

class QOpenGLShaderProgram;

namespace PlottingOpenGL {

class ComputeShaderIntersector : public CorePlotting::ILineBatchIntersector {
public:
    /**
     * @brief Construct a ComputeShaderIntersector backed by the given store.
     *
     * @param store The BatchLineStore that owns the segment SSBOs.
     *              The store must outlive this intersector.
     */
    explicit ComputeShaderIntersector(BatchLineStore & store);
    ~ComputeShaderIntersector() override;

    ComputeShaderIntersector(ComputeShaderIntersector const &) = delete;
    ComputeShaderIntersector & operator=(ComputeShaderIntersector const &) = delete;

    /**
     * @brief Load and compile the compute shader program.
     *
     * Must be called with a current OpenGL 4.3+ context.
     * Uses ShaderManager to manage the program lifetime per-context.
     *
     * @param shader_resource_path  Path to the .comp shader (file or Qt resource).
     *                              Defaults to ":/shaders/line_intersection.comp".
     * @return true if the shader compiled and linked successfully.
     */
    [[nodiscard]] bool initialize(
        std::string const & shader_resource_path = ":/shaders/line_intersection.comp");

    /// Release the shader program (managed by ShaderManager, so this is a no-op
    /// on the program itself, but resets internal state).
    void cleanup();

    /// @return true if initialize() succeeded and the compute shader is usable.
    [[nodiscard]] bool isAvailable() const { return m_initialized; }

    /**
     * @brief Find all lines whose segments intersect the query line.
     *
     * Dispatches the compute shader in batches respecting hardware work-group
     * limits, then reads back results via buffer mapping. The number of results
     * is capped at BatchLineStore::RESULTS_CAPACITY.
     *
     * Returns an empty result (not an error) if:
     *   - !isAvailable() or the compute program is null
     *   - The store's CPU data is empty (no upload() has been called)
     *   - GLFunctions::getExtra() returns null after dispatch
     *   - The compute shader produces zero intersection hits
     *   - m_store is not initialized (bindForCompute() silently no-ops)
     *
     * @note The @p batch parameter is unused — segment data is read from the
     *       store's SSBOs. The parameter exists to satisfy the interface.
     *       The CPU-side data from the store is used for result mapping.
     *
     * @param batch  (Ignored in GPU path; interface conformance only.)
     * @param query  The query line and transform parameters.
     * @return Indices of intersected lines (0-based into LineBatchData::lines).
     *
     * @pre initialize() must have been called and returned true
     *      (enforcement: runtime_check — returns empty result if !m_initialized)
     * @pre A valid OpenGL context must be current on the calling thread
     *      (enforcement: none) [CRITICAL]
     *      — resetIntersectionCount(), dispatchBatched(), glMemoryBarrier(),
     *      glMapBufferRange() all issue raw GL calls. initialize() checks the
     *      context at setup time but does not re-check at call time.
     * @pre m_store.isInitialized() == true (enforcement: none) [IMPORTANT]
     *      — if the store's SSBOs are not created, bindForCompute() silently
     *      no-ops, the compute shader runs with unbound SSBOs, and an empty
     *      result is returned with no error. The caller cannot distinguish
     *      "no intersections" from "store not ready".
     * @pre m_store.upload() must have been called at least once
     *      (enforcement: runtime_check — silent, via cpu.empty() guard)
     *      — if no data has been uploaded, the function returns an empty result
     *      before dispatch. The CPU mirror check provides the guard, but gives
     *      no diagnostic to distinguish "no data" from "no hits".
     * @pre query.start_ndc != query.end_ndc (query line must be non-degenerate)
     *      (enforcement: none) [LOW]
     *      — a zero-length query line is sent to the shader as-is and will
     *      produce no intersections (or undefined shader behaviour).
     * @pre query.tolerance > 0.0f (enforcement: none) [LOW]
     *      — zero or negative tolerance means no segment can be "within"
     *      the threshold; all queries return empty results silently.
     * @pre m_store must remain valid (not destroyed) for the lifetime of this
     *      intersector (enforcement: none — by construction contract on ctor)
     */
    [[nodiscard]] CorePlotting::LineIntersectionResult intersect(
        CorePlotting::LineBatchData const & batch,
        CorePlotting::LineIntersectionQuery const & query) const override;

private:
    BatchLineStore & m_store;
    QOpenGLShaderProgram * m_compute_program{nullptr};
    bool m_initialized{false};

    /// Work-group local size (must match the shader's layout(local_size_x = 64)).
    static constexpr std::uint32_t LOCAL_SIZE_X = 64;

    /// Name used with ShaderManager for per-context program management.
    static constexpr char const * SHADER_PROGRAM_NAME = "batch_line_intersection_compute";

    /**
     * @brief Dispatch the compute shader in batches.
     *
     * Handles hardware work-group count limits by splitting the dispatch into
     * multiple glDispatchCompute calls with offset/batch-size uniforms.
     */
    void dispatchBatched(
        std::uint32_t num_segments,
        CorePlotting::LineIntersectionQuery const & query) const;
};

} // namespace PlottingOpenGL

#endif // PLOTTINGOPENGL_LINEBATCH_COMPUTESHADERINTERSECTOR_HPP
