#include "AxisMapping.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace CorePlotting {

namespace {

/// Format a double to a fixed-precision string, trimming trailing zeros.
std::string formatDecimal(double value, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        // Trim trailing zeros
        auto last_nonzero = s.find_last_not_of('0');
        if (last_nonzero != std::string::npos && s[last_nonzero] == '.') {
            s.erase(last_nonzero);  // Remove the trailing dot too
        } else {
            s.erase(last_nonzero + 1);
        }
    }
    return s;
}

}  // anonymous namespace

// =============================================================================
// identityAxis
// =============================================================================

AxisMapping identityAxis(std::string title, int decimals) {
    return AxisMapping{
        .worldToDomain = [](double w) { return w; },
        .domainToWorld = [](double d) { return d; },
        .formatLabel = [decimals](double d) { return formatDecimal(d, decimals); },
        .title = std::move(title),
    };
}

// =============================================================================
// linearAxis
// =============================================================================

AxisMapping linearAxis(double scale, double offset,
                       std::string title, int decimals) {
    return AxisMapping{
        .worldToDomain = [scale, offset](double w) { return w * scale + offset; },
        .domainToWorld = [scale, offset](double d) { return (d - offset) / scale; },
        .formatLabel = [decimals](double d) { return formatDecimal(d, decimals); },
        .title = std::move(title),
    };
}

// =============================================================================
// trialIndexAxis
// =============================================================================

AxisMapping trialIndexAxis(std::size_t trial_count) {
    auto const count = static_cast<double>(trial_count);

    return AxisMapping{
        // world ∈ [-1, 1] → trial ∈ [0, trial_count)
        .worldToDomain = [count](double w) {
            return (w + 1.0) / 2.0 * count;
        },
        // trial → world
        .domainToWorld = [count](double d) {
            return d / count * 2.0 - 1.0;
        },
        // Integer labels: "0", "1", "42"
        .formatLabel = [](double d) {
            auto const idx = static_cast<long long>(std::round(d));
            return std::to_string(idx);
        },
        .title = "Trial",
    };
}

// =============================================================================
// relativeTimeAxis
// =============================================================================

AxisMapping relativeTimeAxis() {
    return AxisMapping{
        .worldToDomain = [](double w) { return w; },
        .domainToWorld = [](double d) { return d; },
        .formatLabel = [](double d) -> std::string {
            auto const ms = static_cast<long long>(std::round(d));
            if (ms == 0) {
                return "0";
            }
            if (ms > 0) {
                return "+" + std::to_string(ms);
            }
            return std::to_string(ms);  // negative sign included
        },
        .title = "Time (ms)",
    };
}

// =============================================================================
// analogAxis
// =============================================================================

AxisMapping analogAxis(double gain, double offset,
                       std::string unit, int decimals) {
    return AxisMapping{
        .worldToDomain = [gain, offset](double w) { return w * gain + offset; },
        .domainToWorld = [gain, offset](double d) { return (d - offset) / gain; },
        .formatLabel = [unit, decimals](double d) {
            return formatDecimal(d, decimals) + " " + unit;
        },
        .title = std::move(unit),
    };
}

}  // namespace CorePlotting
