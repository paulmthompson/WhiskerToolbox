#include "ParameterFactory.hpp"
#include "Lines/line_alignment.hpp"
#include "Lines/line_resample.hpp"
#include "Lines/line_angle.hpp"
#include "Masks/mask_median_filter.hpp"
#include "Masks/mask_area.hpp"

#include <iostream>

void ParameterFactory::registerParameterSetter(std::string const& transform_name,
                                               std::string const& param_name,
                                               ParameterSetter setter) {
    setters_[transform_name][param_name] = std::move(setter);
}

bool ParameterFactory::setParameter(std::string const& transform_name,
                                   TransformParametersBase* param_obj,
                                   std::string const& param_name,
                                   nlohmann::json const& json_value,
                                   DataManager* data_manager) {
    auto transform_it = setters_.find(transform_name);
    if (transform_it == setters_.end()) {
        std::cerr << "No parameter setters registered for transform: " << transform_name << std::endl;
        return false;
    }
    
    auto param_it = transform_it->second.find(param_name);
    if (param_it == transform_it->second.end()) {
        std::cerr << "No setter registered for parameter '" << param_name 
                  << "' in transform '" << transform_name << "'" << std::endl;
        return false;
    }
    
    return param_it->second(param_obj, json_value, data_manager);
}

ParameterFactory& ParameterFactory::getInstance() {
    static ParameterFactory instance;
    return instance;
}

void ParameterFactory::initializeDefaultSetters() {
    // Register LineAlignment parameters
    registerDataParameter<LineAlignmentParameters, MediaData>(
        "Line Alignment", "media_data", &LineAlignmentParameters::media_data);
    
    registerBasicParameter<LineAlignmentParameters, int>(
        "Line Alignment", "width", &LineAlignmentParameters::width);
    
    registerBasicParameter<LineAlignmentParameters, int>(
        "Line Alignment", "perpendicular_range", &LineAlignmentParameters::perpendicular_range);
    
    registerBasicParameter<LineAlignmentParameters, bool>(
        "Line Alignment", "use_processed_data", &LineAlignmentParameters::use_processed_data);
    
    // Register FWHM approach enum for LineAlignment
    std::unordered_map<std::string, FWHMApproach> fwhm_approach_map = {
        {"PEAK_WIDTH_HALF_MAX", FWHMApproach::PEAK_WIDTH_HALF_MAX},
        {"GAUSSIAN_FIT", FWHMApproach::GAUSSIAN_FIT},
        {"THRESHOLD_BASED", FWHMApproach::THRESHOLD_BASED}
    };
    registerEnumParameter<LineAlignmentParameters, FWHMApproach>(
        "Line Alignment", "approach", &LineAlignmentParameters::approach, fwhm_approach_map);
    
    // Example: Register LineResample parameters (uncomment when parameters exist)
    // registerBasicParameter<LineResampleParameters, int>(
    //     "Line Resample", "num_points", &LineResampleParameters::num_points);
    // registerBasicParameter<LineResampleParameters, bool>(
    //     "Line Resample", "preserve_endpoints", &LineResampleParameters::preserve_endpoints);
    
    // Example: Register LineAngle parameters
    // registerBasicParameter<LineAngleParameters, float>(
    //     "Line Angle", "segment_length", &LineAngleParameters::segment_length);
    // registerBasicParameter<LineAngleParameters, int>(
    //     "Line Angle", "smoothing_window", &LineAngleParameters::smoothing_window);
    
    // Example: Register MaskMedianFilter parameters
    // registerBasicParameter<MaskMedianFilterParameters, int>(
    //     "Mask Median Filter", "kernel_size", &MaskMedianFilterParameters::kernel_size);
    // registerBasicParameter<MaskMedianFilterParameters, int>(
    //     "Mask Median Filter", "iterations", &MaskMedianFilterParameters::iterations);
    
    // MaskArea operation has no configurable parameters
    // It will work automatically when referenced in JSON with operation: "Calculate Area"
    
    std::cout << "Parameter factory initialized with default setters" << std::endl;
}
