/**
 * @file SVGDocument.cpp
 * @brief SVG XML assembly for PlottingSVG exports.
 */

#include "PlottingSVG/SVGDocument.hpp"

#include <algorithm>
#include <sstream>

namespace PlottingSVG {

SVGDocument::SVGDocument(int width,// NOLINT(bugprone-easily-swappable-parameters)
                         int height)
    : _width(width),
      _height(height) {
}

void SVGDocument::setBackground(std::string const & hex_color) {
    _background_hex = hex_color;
}

void SVGDocument::setDescription(std::string const & desc) {
    _description = desc;
}

void SVGDocument::addElements(std::string const & layer_name,
                              std::vector<std::string> const & elements) {
    auto const it = std::ranges::find(_layer_order, layer_name);
    if (it == _layer_order.end()) {
        _layer_order.push_back(layer_name);
        _layer_elements.push_back(elements);
        return;
    }
    auto const index = static_cast<std::size_t>(std::distance(_layer_order.begin(), it));
    auto & dest = _layer_elements.at(index);
    dest.insert(dest.end(), elements.begin(), elements.end());
}

std::string SVGDocument::build() const {
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    oss << "width=\"" << _width << "\" height=\"" << _height << "\" ";
    oss << "viewBox=\"0 0 " << _width << " " << _height << "\">\n";
    if (!_description.empty()) {
        oss << "  <desc>" << _description << "</desc>\n";
    }
    oss << R"(  <rect width="100%" height="100%" fill=")" << _background_hex << R"("/>)" << '\n';
    for (std::size_t i = 0; i < _layer_order.size(); ++i) {
        oss << "  <g id=\"" << _layer_order[i] << "\">\n";
        for (std::string const & element: _layer_elements.at(i)) {
            oss << "    " << element << '\n';
        }
        oss << "  </g>\n";
    }
    oss << "</svg>\n";
    return oss.str();
}

}// namespace PlottingSVG
