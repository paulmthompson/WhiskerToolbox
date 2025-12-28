#ifndef PLOTTINGOPENGL_RENDERERS_AXISRENDERER_HPP
#define PLOTTINGOPENGL_RENDERERS_AXISRENDERER_HPP

/**
 * @file AxisRenderer.hpp
 * @brief OpenGL renderer for axis lines and grid overlays
 * 
 * This renderer provides axis and grid line rendering for time series plots.
 * It supports:
 * - Solid axis lines (vertical axis at x=0)
 * - Dashed grid lines at regular time intervals
 * 
 * @section Architecture
 * 
 * **Stage 1 (current):** Extracted from OpenGLWidget for separation of concerns.
 * Renders based on explicit configuration structs.
 * 
 * **Stage 2 (planned):** Will consume axis/grid configuration from CorePlotting
 * scene graph, enabling backend-agnostic scene description and SVG export.
 * 
 * @section Usage
 * 
 * @code
 * // In initializeGL():
 * AxisRenderer renderer;
 * renderer.initialize();
 * 
 * // In paintGL():
 * AxisConfig axis_config;
 * axis_config.x_position = 0.0f;
 * axis_config.y_min = view_state.y_min;
 * axis_config.y_max = view_state.y_max;
 * axis_config.color = {1.0f, 1.0f, 1.0f};
 * renderer.renderAxis(axis_config, view, projection);
 * 
 * GridConfig grid_config;
 * grid_config.time_start = view_state.time_start;
 * grid_config.time_end = view_state.time_end;
 * grid_config.spacing = 100;
 * grid_config.y_min = view_state.y_min;
 * grid_config.y_max = view_state.y_max;
 * renderer.renderGrid(grid_config, view, projection, viewport_width, viewport_height);
 * 
 * // Before context destruction:
 * renderer.cleanup();
 * @endcode
 * 
 * @see CorePlotting scene graph (Stage 2 integration target)
 */

#include "GLContext.hpp"
#include "IBatchRenderer.hpp"

#include <glm/glm.hpp>

#include <cstdint>

namespace PlottingOpenGL {

/**
 * @brief Configuration for rendering a vertical axis line
 * 
 * @note Stage 2 TODO: Move to CorePlotting scene graph as AxisDescription
 */
struct AxisConfig {
    float x_position{0.0f};         ///< X position of the vertical axis (typically 0)
    float y_min{-1.0f};             ///< Y coordinate of bottom of axis
    float y_max{1.0f};              ///< Y coordinate of top of axis
    glm::vec3 color{1.0f, 1.0f, 1.0f}; ///< Line color (RGB)
    float alpha{1.0f};              ///< Line opacity
    float line_width{1.0f};         ///< Line width in pixels
};

/**
 * @brief Configuration for rendering time-aligned grid lines
 * 
 * Grid lines are vertical dashed lines drawn at regular time intervals.
 * 
 * @note Stage 2 TODO: Move to CorePlotting scene graph as GridDescription
 */
struct GridConfig {
    int64_t time_start{0};          ///< Start of visible time range
    int64_t time_end{1000};         ///< End of visible time range
    int64_t spacing{100};           ///< Spacing between grid lines (in time units)
    float y_min{-1.0f};             ///< Y coordinate of bottom of grid lines
    float y_max{1.0f};              ///< Y coordinate of top of grid lines
    glm::vec3 color{0.5f, 0.5f, 0.5f}; ///< Grid line color (RGB)
    float alpha{0.5f};              ///< Grid line opacity
    float dash_length{3.0f};        ///< Length of each dash in pixels
    float gap_length{3.0f};         ///< Gap between dashes in pixels
};

/**
 * @brief Renders axis lines and grid overlays using OpenGL
 * 
 * This renderer manages its own shader programs:
 * - "axes" shader for solid lines (from ShaderManager)
 * - "dashed_line" shader for grid lines (from ShaderManager)
 * 
 * Unlike other batch renderers, AxisRenderer does not implement IBatchRenderer
 * because it doesn't receive data from RenderableScene batches. Instead, it
 * renders directly from configuration structs.
 * 
 * @note The shaders must be loaded into ShaderManager before calling render methods.
 *       Typically done by the hosting widget in initializeGL().
 */
class AxisRenderer {
public:
    AxisRenderer();
    ~AxisRenderer();

    // Non-copyable, non-movable (GPU resources)
    AxisRenderer(AxisRenderer const &) = delete;
    AxisRenderer & operator=(AxisRenderer const &) = delete;
    AxisRenderer(AxisRenderer &&) = delete;
    AxisRenderer & operator=(AxisRenderer &&) = delete;

    /**
     * @brief Initialize GPU resources (VAO, VBO)
     * 
     * Must be called with a valid OpenGL context.
     * Assumes shaders are already loaded in ShaderManager.
     * 
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize();

    /**
     * @brief Release all GPU resources
     * 
     * Safe to call multiple times or without prior initialize().
     */
    void cleanup();

    /**
     * @brief Check if the renderer has been successfully initialized
     */
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Render a vertical axis line
     * 
     * @param config Axis configuration (position, bounds, color)
     * @param view View matrix
     * @param projection Projection matrix
     */
    void renderAxis(AxisConfig const & config,
                    glm::mat4 const & view,
                    glm::mat4 const & projection);

    /**
     * @brief Render grid lines at regular time intervals
     * 
     * @param config Grid configuration (time range, spacing, style)
     * @param view View matrix
     * @param projection Projection matrix
     * @param viewport_width Viewport width in pixels (for dashed line shader)
     * @param viewport_height Viewport height in pixels (for dashed line shader)
     */
    void renderGrid(GridConfig const & config,
                    glm::mat4 const & view,
                    glm::mat4 const & projection,
                    int viewport_width,
                    int viewport_height);

private:
    /**
     * @brief Draw a single dashed line
     * 
     * @param x_start Start X coordinate (normalized)
     * @param y_start Start Y coordinate
     * @param x_end End X coordinate (normalized)
     * @param y_end End Y coordinate
     * @param mvp Combined Model-View-Projection matrix
     * @param viewport_width Viewport width in pixels
     * @param viewport_height Viewport height in pixels
     * @param dash_length Dash length in pixels
     * @param gap_length Gap length in pixels
     */
    void renderDashedLine(float x_start, float y_start,
                          float x_end, float y_end,
                          glm::mat4 const & mvp,
                          int viewport_width, int viewport_height,
                          float dash_length, float gap_length);

    GLVertexArray m_vao;
    GLBuffer m_vbo{GLBuffer::Type::Vertex};

    // Cached uniform locations for axes shader
    int m_axes_proj_loc{-1};
    int m_axes_view_loc{-1};
    int m_axes_model_loc{-1};
    int m_axes_color_loc{-1};
    int m_axes_alpha_loc{-1};

    // Cached uniform locations for dashed line shader
    int m_dashed_mvp_loc{-1};
    int m_dashed_resolution_loc{-1};
    int m_dashed_dash_size_loc{-1};
    int m_dashed_gap_size_loc{-1};

    bool m_initialized{false};
};

}// namespace PlottingOpenGL

#endif// PLOTTINGOPENGL_RENDERERS_AXISRENDERER_HPP
