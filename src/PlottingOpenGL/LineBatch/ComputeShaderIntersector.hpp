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
     * limits, reads back results via buffer mapping.
     *
     * @note The @p batch parameter is unused — segment data is read from the
     *       store's SSBOs.  The parameter exists to satisfy the interface.
     *       The CPU-side data from the store is used for result mapping.
     *
     * @param batch  (Ignored in GPU path; interface conformance.)
     * @param query  The query line and transform parameters.
     * @return Indices of intersected lines (0-based into LineBatchData::lines).
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
