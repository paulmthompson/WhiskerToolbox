#ifndef DISPLAY_OPTIONS_HPP
#define DISPLAY_OPTIONS_HPP

#include "utils/color.hpp"

#include <string>
#include <vector> 

namespace DefaultDisplayValues {
    const std::string COLOR = "#007bff";
    const float ALPHA = 1.0f;
    const bool VISIBLE = false;
    const int POINT_SIZE = 5;
    const int LINE_THICKNESS = 2;
    const int TENSOR_DISPLAY_CHANNEL = 0;
    const bool SHOW_POINTS = false;
    
    const std::vector<std::string> DEFAULT_COLORS = {
        "#ff0000", // Red
        "#008000", // Green
        "#00ffff", // Cyan
        "#ff00ff", // Magenta
        "#ffff00"  // Yellow
    };
    
    // Get color from index, returns random color if index exceeds DEFAULT_COLORS size
    inline std::string getColorForIndex(size_t index) {
        if (index < DEFAULT_COLORS.size()) {
            return DEFAULT_COLORS[index];
        } else {
            return generateRandomColor();
        }
    }
}

struct BaseDisplayOptions {
    std::string hex_color{DefaultDisplayValues::COLOR};
    float alpha{DefaultDisplayValues::ALPHA};
    bool is_visible{DefaultDisplayValues::VISIBLE};

    virtual ~BaseDisplayOptions() = default;

};

struct PointDisplayOptions : public BaseDisplayOptions {
    int point_size{DefaultDisplayValues::POINT_SIZE};
    // Future: point_shape (e.g., circle, square, triangle enum)

    // OptionType getType() const override { return OptionType::Point; }
};

struct LineDisplayOptions : public BaseDisplayOptions {
    int line_thickness{DefaultDisplayValues::LINE_THICKNESS};
    bool show_points{DefaultDisplayValues::SHOW_POINTS}; // Show points as open circles along the line
    // Future: line_style (e.g., solid, dashed, dotted enum)

    // OptionType getType() const override { return OptionType::Line; }
};

struct MaskDisplayOptions : public BaseDisplayOptions {

    // OptionType getType() const override { return OptionType::Mask; }
};

struct TensorDisplayOptions : public BaseDisplayOptions {
    int display_channel{DefaultDisplayValues::TENSOR_DISPLAY_CHANNEL};

    // OptionType getType() const override { return OptionType::Tensor; }
};

struct DigitalIntervalDisplayOptions : public BaseDisplayOptions {
    // OptionType getType() const override { return OptionType::DigitalInterval; }
};

#endif // DISPLAY_OPTIONS_HPP
