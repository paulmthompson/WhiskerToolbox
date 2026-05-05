/**
 * @file GroupViewerUniqueRandomColors.cpp
 * @brief Shared hue-wheel color generation for DataViewer group property widgets.
 */

#include "GroupViewerUniqueRandomColors.hpp"

#include <QColor>
#include <QRandomGenerator>

#include <cmath>

std::vector<std::string> uniqueRandomHueWheelHexColors(std::size_t const series_count) {
    if (series_count == 0) {
        return {};
    }

    int const n = static_cast<int>(series_count);
    float const hue0 =
            static_cast<float>(QRandomGenerator::global()->bounded(10'000)) / 10'000.0f;
    float constexpr saturation = 0.82f;
    float constexpr value = 0.92f;

    std::vector<std::string> out;
    out.reserve(series_count);
    for (int i = 0; i < n; ++i) {
        float const hue = std::fmod(hue0 + static_cast<float>(i) / static_cast<float>(n), 1.0f);
        QColor const color = QColor::fromHsvF(hue, saturation, value);
        out.push_back(color.name().toStdString());
    }
    return out;
}
