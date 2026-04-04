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

/// Orchestrates per-batch SVG renderers and optional decorations.
class SVGSceneRenderer {
public:
    SVGSceneRenderer();

    /// @pre The referenced scene must outlive calls to `render()` / `renderToFile()`.
    void setScene(CorePlotting::RenderableScene const & scene);

    void setCanvasSize(int width, int height);

    void setBackgroundColor(std::string const & hex_color);

    void addDecoration(std::unique_ptr<SVGDecoration> decoration);

    void clearDecorations();

    [[nodiscard]] std::string render() const;

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
