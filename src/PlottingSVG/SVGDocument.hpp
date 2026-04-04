#ifndef PLOTTINGSVG_SVGDOCUMENT_HPP
#define PLOTTINGSVG_SVGDOCUMENT_HPP

/**
 * @file SVGDocument.hpp
 * @brief Assembles a complete SVG document from layer groups and metadata.
 */

#include <string>
#include <vector>

namespace PlottingSVG {

/// Builds a well-formed SVG 1.1 document string with optional background and layers.
class SVGDocument {
public:
    explicit SVGDocument(int width, int height);

    /// @brief Set solid background fill using an SVG color (e.g. `#RRGGBB`).
    void setBackground(std::string const & hex_color);

    /// @brief Optional prose description embedded in `<desc>`.
    void setDescription(std::string const & desc);

    /// @brief Append SVG fragments to a named `<g id="...">` layer.
    void addElements(std::string const & layer_name,
                     std::vector<std::string> const & elements);

    /// @return Full XML document including declaration and root `<svg>`.
    [[nodiscard]] std::string build() const;

private:
    int _width{};
    int _height{};
    std::string _background_hex{"#FFFFFF"};
    std::string _description{"WhiskerToolbox Export"};
    /// Layer names in insertion order; each maps to accumulated element strings.
    std::vector<std::string> _layer_order;
    std::vector<std::vector<std::string>> _layer_elements;
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGDOCUMENT_HPP
