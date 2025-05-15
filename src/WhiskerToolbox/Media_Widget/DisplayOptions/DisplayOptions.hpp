#ifndef DISPLAY_OPTIONS_HPP
#define DISPLAY_OPTIONS_HPP

#include <string>
#include <vector> 

namespace DefaultDisplayValues {
    const std::string COLOR = "#007bff";
    const float ALPHA = 1.0f;
    const bool VISIBLE = false;
    const int POINT_SIZE = 5;
    const int LINE_THICKNESS = 2;
    const int TENSOR_DISPLAY_CHANNEL = 0;
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
