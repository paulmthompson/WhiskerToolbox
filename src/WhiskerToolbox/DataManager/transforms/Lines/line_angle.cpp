#include "line_angle.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Lines/Line_Data.hpp"

#include <cmath>
#include <map>
#include <numbers>

std::shared_ptr<AnalogTimeSeries> line_angle(LineData const * line_data, LineAngleParameters const * params) {
    auto analog_time_series = std::make_shared<AnalogTimeSeries>();
    std::map<int, float> angles;
    
    // Use default position if no parameters provided
    float position = params ? params->position : 0.2f;
    
    // Ensure position is within valid range
    position = std::max(0.0f, std::min(position, 1.0f));
    
    for (auto const & line_and_time : line_data->GetAllLinesAsRange()) {

        if (line_and_time.lines.empty()) {
            continue;
        }
        
        Line2D const & line = line_and_time.lines[0];
        
        if (line.size() < 2) {
            continue;
        }
        
        // Calculate the index of the position point
        auto idx = static_cast<size_t>(position * static_cast<float>((line.size() - 1)));
        if (idx >= line.size()) {
            idx = line.size() - 1;
        }
        
        Point2D<float> const base = line[0];
        Point2D<float> const pos = line[idx];
        
        float const angle = std::atan2(pos.y - base.y, pos.x - base.x);

        angles[line_and_time.time] = angle * 180.0f / std::numbers::pi;
    }
    
    analog_time_series->setData(angles);
    return analog_time_series;
}

std::string LineAngleOperation::getName() const {
    return "Calculate Line Angle";
}

std::type_index LineAngleOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineAngleOperation::canApply(DataTypeVariant const & dataVariant) const {

    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> LineAngleOperation::getDefaultParameters() const {
    return std::make_unique<LineAngleParameters>();
}

DataTypeVariant LineAngleOperation::execute(DataTypeVariant const & dataVariant, 
                                           TransformParametersBase const * transformParameters) {
                                               
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "LineAngleOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};  // Return empty variant
    }
    
    LineData const * line_raw_ptr = (*ptr_ptr).get();
    
    LineAngleParameters const * typed_params = nullptr;
    if (transformParameters) {
        typed_params = dynamic_cast<LineAngleParameters const *>(transformParameters);
        if (!typed_params) {
            std::cerr << "LineAngleOperation::execute: Invalid parameter type" << std::endl;
        }
    }
    
    std::shared_ptr<AnalogTimeSeries> result_ts = line_angle(line_raw_ptr, typed_params);
    
    // Handle potential failure from the calculation function
    if (!result_ts) {
        std::cerr << "LineAngleOperation::execute: 'line_angle' failed to produce a result." << std::endl;
        return {};  // Return empty variant
    }
    
    std::cout << "LineAngleOperation executed successfully using variant input." << std::endl;
    return result_ts;
}
