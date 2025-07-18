#ifndef DISPLAY_OPTIONS_HPP
#define DISPLAY_OPTIONS_HPP

#include "../../DataManager/utils/color.hpp"

#include <string>
#include <vector>

/**
 * @brief Enumeration for different point marker shapes
 */
enum class PointMarkerShape {
    Circle,  ///< Circular marker (filled ellipse)
    Square,  ///< Square marker (filled rectangle)
    Triangle,///< Triangular marker (filled triangle)
    Cross,   ///< Cross/plus marker (+ shape)
    X,       ///< X marker (× shape)
    Diamond  ///< Diamond marker (rotated square)
};

namespace DefaultDisplayValues {
std::string const COLOR = "#007bff";
float const ALPHA = 1.0f;
bool const VISIBLE = false;
int const POINT_SIZE = 5;
int const LINE_THICKNESS = 2;
int const TENSOR_DISPLAY_CHANNEL = 0;
bool const SHOW_POINTS = false;
PointMarkerShape const POINT_MARKER_SHAPE = PointMarkerShape::Circle;

std::vector<std::string> const DEFAULT_COLORS = {
        "#ff0000",// Red
        "#008000",// Green
        "#00ffff",// Cyan
        "#ff00ff",// Magenta
        "#ffff00" // Yellow
};

// Get color from index, returns random color if index exceeds DEFAULT_COLORS size
inline std::string getColorForIndex(size_t index) {
    if (index < DEFAULT_COLORS.size()) {
        return DEFAULT_COLORS[index];
    } else {
        return generateRandomColor();
    }
}
}// namespace DefaultDisplayValues

struct BaseDisplayOptions {
    std::string hex_color{DefaultDisplayValues::COLOR};
    float alpha{DefaultDisplayValues::ALPHA};
    bool is_visible{DefaultDisplayValues::VISIBLE};

    virtual ~BaseDisplayOptions() = default;
};

struct PointDisplayOptions : public BaseDisplayOptions {
    int point_size{DefaultDisplayValues::POINT_SIZE};
    PointMarkerShape marker_shape{DefaultDisplayValues::POINT_MARKER_SHAPE};

    // OptionType getType() const override { return OptionType::Point; }
};

struct LineDisplayOptions : public BaseDisplayOptions {
    int line_thickness{DefaultDisplayValues::LINE_THICKNESS};
    bool show_points{DefaultDisplayValues::SHOW_POINTS};// Show points as open circles along the line
    bool edge_snapping{false};                          // Enable edge snapping for new points
    bool show_position_marker{false};                   // Show position marker at percentage distance
    int position_percentage{20};                        // Percentage distance along line (0-100%)
    bool show_segment{false};                           // Show only a segment of the line between two percentages
    int segment_start_percentage{0};                    // Start percentage for line segment (0-100%)
    int segment_end_percentage{100};                    // End percentage for line segment (0-100%)
    int selected_line_index{-1};                        // Index of selected line (-1 = none selected)
    // Future: line_style (e.g., solid, dashed, dotted enum)

    // OptionType getType() const override { return OptionType::Line; }
};

struct MaskDisplayOptions : public BaseDisplayOptions {
    bool show_bounding_box{false};// Show bounding box around the mask
    bool show_outline{false};     // Show outline of the mask as a thick line
    bool use_as_transparency{false}; // Use mask as transparency layer (invert display)

    // OptionType getType() const override { return OptionType::Mask; }
};

struct TensorDisplayOptions : public BaseDisplayOptions {
    int display_channel{DefaultDisplayValues::TENSOR_DISPLAY_CHANNEL};

    // OptionType getType() const override { return OptionType::Tensor; }
};

enum class IntervalPlottingStyle {
    Box,        // Box indicators in corners
    Border,     // Border around entire image
    // Future styles can be added here
};

enum class IntervalLocation {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct DigitalIntervalDisplayOptions : public BaseDisplayOptions {
    IntervalPlottingStyle plotting_style{IntervalPlottingStyle::Box}; // Overall plotting style
    
    // Box style specific options
    int box_size{20};                                     // Size of each interval box in pixels
    int frame_range{2};                                   // Number of frames before/after current (+/- range)
    IntervalLocation location{IntervalLocation::TopRight};// Location of interval boxes
    
    // Border style specific options
    int border_thickness{5};                              // Thickness of border in pixels

    // OptionType getType() const override { return OptionType::DigitalInterval; }
};

#endif// DISPLAY_OPTIONS_HPP
