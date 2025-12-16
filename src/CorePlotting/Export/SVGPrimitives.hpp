#ifndef COREPLOTTING_EXPORT_SVGPRIMITIVES_HPP
#define COREPLOTTING_EXPORT_SVGPRIMITIVES_HPP

/**
 * @file SVGPrimitives.hpp
 * @brief SVG export functions for CorePlotting RenderableBatch types.
 * 
 * These functions convert RenderableBatch objects (the same data used for OpenGL rendering)
 * into SVG elements. This ensures that SVG exports match the on-screen visualization exactly,
 * as they use the same batch data and transformation matrices.
 * 
 * Architecture:
 * - Each function takes a batch (with its Model matrix) plus shared View/Projection matrices
 * - Applies MVP transformation to convert world coordinates to NDC
 * - Maps NDC to SVG canvas coordinates
 * - Returns SVG element strings ready for document assembly
 * 
 * Usage:
 * @code
 *   CorePlotting::SVGExportParams params{1920, 1080};
 *   auto svg_elements = renderPolyLineBatchToSVG(batch, view_matrix, projection_matrix, params);
 *   for (auto const& elem : svg_elements) {
 *       document += elem;
 *   }
 * @endcode
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace CorePlotting {

/**
 * @brief Parameters for SVG export.
 */
struct SVGExportParams {
    int canvas_width{1920};                 ///< SVG canvas width in pixels
    int canvas_height{1080};                ///< SVG canvas height in pixels
    std::string background_color{"#1E1E1E"};///< Background color hex string
};

/**
 * @brief Transform a vertex from world space to SVG coordinates.
 * 
 * Applies Model-View-Projection transformation to convert data coordinates
 * to normalized device coordinates (NDC), then maps NDC to SVG pixel coordinates.
 * 
 * @param vertex Input vertex in world coordinates (x, y, z, w)
 * @param mvp Combined Model-View-Projection matrix
 * @param canvas_width SVG canvas width in pixels
 * @param canvas_height SVG canvas height in pixels
 * @return 2D point in SVG coordinate space
 */
glm::vec2 transformVertexToSVG(
        glm::vec4 const & vertex,
        glm::mat4 const & mvp,
        int canvas_width,
        int canvas_height);

/**
 * @brief Convert glm::vec4 RGBA color to SVG hex string.
 * 
 * @param color RGBA color with components in [0, 1] range
 * @return Hex color string (e.g., "#FF5733")
 */
std::string colorToSVGHex(glm::vec4 const & color);

/**
 * @brief Render a RenderablePolyLineBatch to SVG polyline elements.
 * 
 * Each line segment in the batch becomes a separate <polyline> element.
 * Uses the batch's Model matrix combined with the shared View/Projection.
 * 
 * @param batch The polyline batch to render
 * @param view_matrix Shared view matrix (camera pan)
 * @param projection_matrix Shared projection matrix (world → NDC)
 * @param params SVG export parameters
 * @return Vector of SVG polyline element strings
 */
std::vector<std::string> renderPolyLineBatchToSVG(
        RenderablePolyLineBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params);

/**
 * @brief Render a RenderableGlyphBatch to SVG elements.
 * 
 * Each glyph becomes an SVG element based on the glyph type:
 * - Tick: <line> element (vertical line)
 * - Circle: <circle> element
 * - Square: <rect> element
 * - Cross: Two <line> elements forming an X
 * 
 * @param batch The glyph batch to render
 * @param view_matrix Shared view matrix
 * @param projection_matrix Shared projection matrix
 * @param params SVG export parameters
 * @return Vector of SVG element strings
 */
std::vector<std::string> renderGlyphBatchToSVG(
        RenderableGlyphBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params);

/**
 * @brief Render a RenderableRectangleBatch to SVG rect elements.
 * 
 * Each rectangle in the batch becomes a <rect> SVG element.
 * 
 * @param batch The rectangle batch to render
 * @param view_matrix Shared view matrix
 * @param projection_matrix Shared projection matrix
 * @param params SVG export parameters
 * @return Vector of SVG rect element strings
 */
std::vector<std::string> renderRectangleBatchToSVG(
        RenderableRectangleBatch const & batch,
        glm::mat4 const & view_matrix,
        glm::mat4 const & projection_matrix,
        SVGExportParams const & params);

/**
 * @brief Render a complete RenderableScene to SVG elements.
 * 
 * Processes all batches in the scene (rectangles first, then polylines, then glyphs)
 * to match the typical rendering order (background → lines → points).
 * 
 * @param scene The scene to render
 * @param params SVG export parameters
 * @return Vector of all SVG element strings in rendering order
 */
std::vector<std::string> renderSceneToSVG(
        RenderableScene const & scene,
        SVGExportParams const & params);

/**
 * @brief Build a complete SVG document from a RenderableScene.
 * 
 * Creates a full SVG document with proper XML header, viewBox, background,
 * and all scene elements.
 * 
 * @param scene The scene to render
 * @param params SVG export parameters
 * @return Complete SVG document as a string
 */
std::string buildSVGDocument(
        RenderableScene const & scene,
        SVGExportParams const & params);

/**
 * @brief Create a scalebar SVG element.
 * 
 * Draws a horizontal scalebar in the bottom-right corner with tick marks
 * and a label showing the length in time units.
 * 
 * @param scalebar_length Length of the scalebar in time units
 * @param time_range_start Visible time range start
 * @param time_range_end Visible time range end
 * @param params SVG export parameters
 * @return Vector of SVG elements for the scalebar
 */
std::vector<std::string> createScalebarSVG(
        int scalebar_length,
        float time_range_start,
        float time_range_end,
        SVGExportParams const & params);

}// namespace CorePlotting

#endif// COREPLOTTING_EXPORT_SVGPRIMITIVES_HPP
