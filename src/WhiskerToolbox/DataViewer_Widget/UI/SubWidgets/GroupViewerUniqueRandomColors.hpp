/**
 * @file GroupViewerUniqueRandomColors.hpp
 * @brief Shared hue-wheel color generation for DataViewer group property widgets.
 */

#ifndef GROUPVIEWER_UNIQUE_RANDOM_COLORS_HPP
#define GROUPVIEWER_UNIQUE_RANDOM_COLORS_HPP

#include <cstddef>
#include <string>
#include <vector>

/**
 * @brief Generate distinct saturated hex colors evenly spaced on the hue wheel.
 *
 * Uses a random starting hue so repeated calls produce different palettes.
 *
 * @param series_count Number of colors to generate
 * @return Hex strings (e.g. "#RRGGBB"); empty if @p series_count is zero
 */
[[nodiscard]] std::vector<std::string> uniqueRandomHueWheelHexColors(std::size_t series_count);

#endif// GROUPVIEWER_UNIQUE_RANDOM_COLORS_HPP
