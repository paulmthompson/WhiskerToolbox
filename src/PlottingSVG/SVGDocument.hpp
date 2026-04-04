#ifndef PLOTTINGSVG_SVGDOCUMENT_HPP
#define PLOTTINGSVG_SVGDOCUMENT_HPP

/**
 * @file SVGDocument.hpp
 * @brief Assembles a complete SVG document from layer groups and metadata.
 */

#include <string>
#include <vector>

namespace PlottingSVG {

/**
 * @brief Builds a UTF-8 SVG 1.1 document string with a full-canvas background and ordered `<g>` layers.
 *
 * Layer content is concatenated verbatim; `build()` does not escape or validate SVG/XML. Callers must
 * supply safe strings (see `setBackground`, `setDescription`, `addElements`, and `build` @pre).
 */
class SVGDocument {
public:
    /**
     * @brief Create a document with pixel width and height used for the root `<svg>` and `viewBox`.
     *
     * @pre `width > 0` and `height > 0` for a conventional pixel canvas and `viewBox="0 0 …"` (enforcement:
     *      none) [IMPORTANT]
     */
    explicit SVGDocument(int width, int height);

    /**
     * @brief Set the full-canvas background rectangle `fill` (SVG paint syntax, typically `#RRGGBB`).
     *
     * @pre `hex_color` must not contain `"`, `&`, or `<` (and otherwise be valid inside a double-quoted XML
     *      attribute). Values are inserted unescaped (enforcement: none) [IMPORTANT]
     */
    void setBackground(std::string const & hex_color);

    /**
     * @brief Set prose inside `<desc>…</desc>`; the tag is omitted only when the stored string is empty.
     *
     * @pre `desc` must not break XML structure (e.g. must not contain a raw `</desc>` sequence or
     *      unescaped `&` / `<` unless intentional). Content is inserted unescaped (enforcement: none)
     *      [IMPORTANT]
     */
    void setDescription(std::string const & desc);

    /**
     * @brief Append SVG fragments into a named layer (`<g id="layer_name">`).
     *
     * If `layer_name` already exists, `elements` are appended to that layer’s vector in order.
     *
     * @pre `layer_name` must be safe inside a double-quoted `id="…"` attribute (no `"`, `&`, `<`, etc.).
     *      Should be a valid `id` token for consumers that enforce XML/SVG id rules (enforcement: none)
     *      [IMPORTANT]
     * @pre Each string in `elements` should be a single SVG element (or fragment) that is valid in this
     *      context; content is written verbatim with indentation only (enforcement: none) [LOW]
     */
    void addElements(std::string const & layer_name,
                     std::vector<std::string> const & elements);

    /**
     * @brief Serialize the accumulated document.
     *
     * @post Returns a string starting with `<?xml version="1.0" encoding="UTF-8"?>`, then a root `<svg>`
     *       with `xmlns`, `width`, `height`, and `viewBox="0 0 width height"` from the constructor.
     * @post When the stored description is non-empty, includes one `<desc>` child with that text
     *       (unescaped).
     * @post Always includes a full-viewport `<rect width="100%" height="100%" fill="…"/>` using the
     *       current background string.
     * @post Emits one `<g id="…">` per distinct `layer_name` in insertion order, each containing all
     *       accumulated elements for that layer, then closes `</svg>`.
     * @post Does not throw under normal `std::ostringstream` behavior.
     */
    [[nodiscard]] std::string build() const;

private:
    int _width{};
    int _height{};
    std::string _background_hex{"#FFFFFF"};
    std::string _description{"WhiskerToolbox Export"};
    /// Layer names in insertion order; parallel index with `_layer_elements`.
    std::vector<std::string> _layer_order;
    std::vector<std::vector<std::string>> _layer_elements;
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGDOCUMENT_HPP
