#ifndef PLOTTINGSVG_SVGSCENERENDERER_HPP
#define PLOTTINGSVG_SVGSCENERENDERER_HPP

/**
 * @file SVGSceneRenderer.hpp
 * @brief Public entry point: RenderableScene to SVG document string.
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Orchestrates `SVGDocument` assembly, per-batch SVG renderers, and optional decorations.
 *
 * Scene geometry is grouped into fixed layer ids: `rectangles`, `polylines`, and `glyphs` (batches are
 * appended in scene vector order within each layer). Non-empty decoration output is added as layer
 * `decorations`. If no scene is set, or the scene pointer is null at `render()` time, the document still
 * contains the default `SVGDocument` metadata and background, with no scene geometry layers.
 */
class SVGSceneRenderer {
public:
    /**
     * @brief Construct a renderer with default canvas size (1920×1080), white background, and no scene.
     */
    SVGSceneRenderer();

    /**
     * @brief Bind the scene whose batches and view/projection matrices are exported on `render()`.
     *
     * The renderer reads `scene.view_matrix` and `scene.projection_matrix` at `render()` time
     * to compute the final MVP (`projection * view * batch.model_matrix`) for every batch.
     * Callers **must** ensure these matrices reflect the desired camera state before calling
     * `render()`. The `RenderableScene` defaults are identity, which maps world coordinates
     * directly to NDC — this is almost never the correct camera for a plot widget. Typically
     * the caller copies the same view/projection matrices used by the live OpenGL renderer
     * onto the scene before exporting.
     *
     * @pre `scene` must remain valid (not destroyed) for every subsequent call to `render()` or
     *      `renderToFile()` while this pointer is still stored (enforcement: none) [CRITICAL]
     * @pre `scene.view_matrix` and `scene.projection_matrix` should reflect the intended camera
     *      transform (enforcement: none — identity produces valid but usually incorrect output)
     *      [IMPORTANT]
     */
    void setScene(CorePlotting::RenderableScene const & scene);

    /**
     * @brief Set the pixel width and height used for the root `<svg>`, `viewBox`, and all renderers.
     *
     * @pre `width > 0` and `height > 0` for a conventional canvas and correct NDC→pixel mapping in batch
     *      renderers (enforcement: none) [IMPORTANT]
     */
    void setCanvasSize(int width, int height);

    /**
     * @brief Set the full-canvas background `fill` passed to `SVGDocument::setBackground`.
     *
     * @pre `hex_color` must be safe inside a double-quoted XML attribute (see `SVGDocument::setBackground`)
     *      (enforcement: none) [IMPORTANT]
     */
    void setBackgroundColor(std::string const & hex_color);

    /**
     * @brief Take ownership of a decoration drawn after scene batches (merged into layer `decorations`).
     *
     * @note `nullptr` is ignored; no layer entry is added for skipped decorations.
     */
    void addDecoration(std::unique_ptr<SVGDecoration> decoration);

    /**
     * @brief Remove and destroy all decorations previously added.
     */
    void clearDecorations();

    /**
     * @brief Build the full SVG document for the current state.
     *
     * @pre If `setScene` was called, the referenced `RenderableScene` must still be alive when `render()`
     *      runs (see `setScene`).
     * @pre Batch contents must satisfy the preconditions of `SVGRectangleRenderer`, `SVGPolyLineRenderer`,
     *      and `SVGGlyphRenderer` (enforcement: none) [LOW]
     * @pre Each decoration’s `render()` must return safe SVG fragments for `SVGDocument::addElements`
     *      (enforcement: none) [LOW]
     *
     * @post Returns `SVGDocument::build()` after configuring background, optional scene layers, and optional
     *       `decorations` layer (only when decoration output is non-empty).
     * @post Does not throw under normal `SVGDocument` and string behavior.
     */
    [[nodiscard]] std::string render() const;

    /**
     * @brief Write `render()` to `path` as UTF-8 bytes (`std::ios::binary`).
     *
     * @pre `path` must be openable for output by this process (parent directory exists, permissions, etc.)
     *      (enforcement: runtime_check — returns `false` on failure) [IMPORTANT]
     *
     * @post Returns `true` if the stream was good after writing the full `render()` result; otherwise
     *       `false` (including when the file could not be opened).
     */
    [[nodiscard]] bool renderToFile(std::filesystem::path const & path) const;

private:
    CorePlotting::RenderableScene const * _scene{nullptr};
    int _canvas_width{1920};
    int _canvas_height{1080};
    std::string _background_hex{"#FFFFFF"};
    std::vector<std::unique_ptr<SVGDecoration>> _decorations;
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGSCENERENDERER_HPP
