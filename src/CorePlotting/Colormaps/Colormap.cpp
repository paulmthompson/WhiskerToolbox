#include "Colormap.hpp"
#include "ColormapLUTs.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace CorePlotting::Colormaps {

// =============================================================================
// Preset names
// =============================================================================

std::string presetName(ColormapPreset preset)
{
    switch (preset) {
        case ColormapPreset::Inferno:   return "Inferno";
        case ColormapPreset::Viridis:   return "Viridis";
        case ColormapPreset::Magma:     return "Magma";
        case ColormapPreset::Plasma:    return "Plasma";
        case ColormapPreset::Coolwarm:  return "Coolwarm";
        case ColormapPreset::Grayscale: return "Grayscale";
        case ColormapPreset::Hot:       return "Hot";
    }
    return "Unknown";
}

std::vector<ColormapPreset> allPresets()
{
    return {
        ColormapPreset::Inferno,
        ColormapPreset::Viridis,
        ColormapPreset::Magma,
        ColormapPreset::Plasma,
        ColormapPreset::Coolwarm,
        ColormapPreset::Grayscale,
        ColormapPreset::Hot,
    };
}

// =============================================================================
// LUT-based colormap function
// =============================================================================

ColormapFunction fromLUT(ColormapLUT const & lut)
{
    // Capture a pointer to the static LUT (avoids copying the 256-entry array)
    return [&lut](float t) -> glm::vec4 {
        // Clamp to [0, 1]
        t = std::clamp(t, 0.0f, 1.0f);

        // Map to LUT index with linear interpolation
        float const scaled = t * 255.0f;
        auto const idx0 = static_cast<std::size_t>(std::floor(scaled));
        auto const idx1 = std::min(idx0 + 1, std::size_t{255});
        float const frac = scaled - static_cast<float>(idx0);

        glm::vec3 const & c0 = lut[idx0];
        glm::vec3 const & c1 = lut[idx1];
        glm::vec3 const rgb = c0 + frac * (c1 - c0);

        return glm::vec4(rgb, 1.0f);
    };
}

// =============================================================================
// Preset → ColormapFunction
// =============================================================================

ColormapFunction getColormap(ColormapPreset preset)
{
    switch (preset) {
        case ColormapPreset::Inferno:   return fromLUT(detail::infernoLUT());
        case ColormapPreset::Viridis:   return fromLUT(detail::viridisLUT());
        case ColormapPreset::Magma:     return fromLUT(detail::magmaLUT());
        case ColormapPreset::Plasma:    return fromLUT(detail::plasmaLUT());
        case ColormapPreset::Coolwarm:  return fromLUT(detail::coolwarmLUT());
        case ColormapPreset::Grayscale: return fromLUT(detail::grayscaleLUT());
        case ColormapPreset::Hot:       return fromLUT(detail::hotLUT());
    }
    return fromLUT(detail::infernoLUT());
}

// =============================================================================
// LUT access
// =============================================================================

ColormapLUT const & getLUT(ColormapPreset preset)
{
    switch (preset) {
        case ColormapPreset::Inferno:   return detail::infernoLUT();
        case ColormapPreset::Viridis:   return detail::viridisLUT();
        case ColormapPreset::Magma:     return detail::magmaLUT();
        case ColormapPreset::Plasma:    return detail::plasmaLUT();
        case ColormapPreset::Coolwarm:  return detail::coolwarmLUT();
        case ColormapPreset::Grayscale: return detail::grayscaleLUT();
        case ColormapPreset::Hot:       return detail::hotLUT();
    }
    return detail::infernoLUT();
}

// =============================================================================
// Mapping utilities
// =============================================================================

glm::vec4 mapValue(
    ColormapFunction const & cmap,
    float value,
    float vmin,
    float vmax)
{
    if (vmax <= vmin) {
        return cmap(0.5f);
    }
    float const t = (value - vmin) / (vmax - vmin);
    return cmap(std::clamp(t, 0.0f, 1.0f));
}

std::vector<glm::vec4> mapValues(
    ColormapFunction const & cmap,
    std::span<double const> values,
    float vmin,
    float vmax)
{
    std::vector<glm::vec4> result;
    result.reserve(values.size());

    for (double const v : values) {
        result.push_back(mapValue(cmap, static_cast<float>(v), vmin, vmax));
    }

    return result;
}

std::vector<glm::vec4> mapMatrix(
    ColormapFunction const & cmap,
    std::span<double const> values,
    [[maybe_unused]] std::size_t num_rows,
    [[maybe_unused]] std::size_t num_cols,
    float vmin,
    float vmax)
{
    return mapValues(cmap, values, vmin, vmax);
}

} // namespace CorePlotting::Colormaps
