#include "ParameterFactory.hpp"

#include "AnalogTimeSeries/AnalogFilter/analog_filter.hpp"
#include "AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"
#include "AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"
#include "AnalogTimeSeries/analog_interval_threshold.hpp"
#include "DigitalIntervalSeries/digital_interval_group.hpp"
#include "Lines/line_alignment.hpp"
#include "Lines/line_angle.hpp"
#include "Lines/line_resample.hpp"
#include "Masks/mask_area.hpp"
#include "Masks/mask_median_filter.hpp"

#include <iostream>

void ParameterFactory::registerParameterSetter(std::string const & transform_name,
                                               std::string const & param_name,
                                               ParameterSetter setter) {
    setters_[transform_name][param_name] = std::move(setter);
}

bool ParameterFactory::setParameter(std::string const & transform_name,
                                    TransformParametersBase * param_obj,
                                    std::string const & param_name,
                                    nlohmann::json const & json_value,
                                    DataManager * data_manager) {
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

ParameterFactory & ParameterFactory::getInstance() {
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
            {"THRESHOLD_BASED", FWHMApproach::THRESHOLD_BASED}};
    registerEnumParameter<LineAlignmentParameters, FWHMApproach>(
            "Line Alignment", "approach", &LineAlignmentParameters::approach, fwhm_approach_map);

    // Register Analog Interval Threshold parameters
    registerBasicParameter<IntervalThresholdParams, double>(
            "Threshold Interval Detection", "threshold_value", &IntervalThresholdParams::thresholdValue);

    std::unordered_map<std::string, IntervalThresholdParams::ThresholdDirection> threshold_direction_map = {
            {"Positive (Rising)", IntervalThresholdParams::ThresholdDirection::POSITIVE},
            {"Negative (Falling)", IntervalThresholdParams::ThresholdDirection::NEGATIVE},
            {"Absolute (Magnitude)", IntervalThresholdParams::ThresholdDirection::ABSOLUTE}};

    registerEnumParameter<IntervalThresholdParams, IntervalThresholdParams::ThresholdDirection>(
            "Threshold Interval Detection", "direction", &IntervalThresholdParams::direction, threshold_direction_map);

    registerBasicParameter<IntervalThresholdParams, double>(
            "Threshold Interval Detection", "lockout_time", &IntervalThresholdParams::lockoutTime);

    registerBasicParameter<IntervalThresholdParams, double>(
            "Threshold Interval Detection", "min_duration", &IntervalThresholdParams::minDuration);

    std::unordered_map<std::string, IntervalThresholdParams::MissingDataMode> missing_data_mode_map = {
            {"Treat as Zero (Default)", IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO},
            {"Ignore Missing Points", IntervalThresholdParams::MissingDataMode::IGNORE}};
    registerEnumParameter<IntervalThresholdParams, IntervalThresholdParams::MissingDataMode>(
            "Threshold Interval Detection", "missing_data_mode", &IntervalThresholdParams::missingDataMode, missing_data_mode_map);

    registerBasicParameter<GroupParams, double>(
            "Group Intervals", "max_spacing", &GroupParams::maxSpacing);

    registerBasicParameter<ThresholdParams, double>(
            "Threshold Event Detection", "threshold_value", &ThresholdParams::thresholdValue);

    std::unordered_map<std::string, ThresholdParams::ThresholdDirection> event_direction_map = {
            {"Positive (Rising)", ThresholdParams::ThresholdDirection::POSITIVE},
            {"Negative (Falling)", ThresholdParams::ThresholdDirection::NEGATIVE},
            {"Absolute (Magnitude)", ThresholdParams::ThresholdDirection::ABSOLUTE}};

    registerEnumParameter<ThresholdParams, ThresholdParams::ThresholdDirection>(
            "Threshold Event Detection", "direction", &ThresholdParams::direction, event_direction_map);

    registerBasicParameter<ThresholdParams, double>(
            "Threshold Event Detection", "lockout_time", &ThresholdParams::lockoutTime);

    // Register Line Resample parameters
    std::unordered_map<std::string, LineSimplificationAlgorithm> line_simplification_map = {
            {"Fixed Spacing", LineSimplificationAlgorithm::FixedSpacing},
            {"Douglas-Peucker", LineSimplificationAlgorithm::DouglasPeucker}};
    registerEnumParameter<LineResampleParameters, LineSimplificationAlgorithm>(
            "Resample Line", "algorithm", &LineResampleParameters::algorithm, line_simplification_map);

    registerBasicParameter<LineResampleParameters, float>(
            "Resample Line", "target_spacing", &LineResampleParameters::target_spacing);

    registerBasicParameter<LineResampleParameters, float>(
            "Resample Line", "epsilon", &LineResampleParameters::epsilon);

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

    // Register Analog Filter parameters
    std::unordered_map<std::string, AnalogFilterParams::FilterType> filter_type_map = {
            {"lowpass", AnalogFilterParams::FilterType::Lowpass},
            {"highpass", AnalogFilterParams::FilterType::Highpass},
            {"bandpass", AnalogFilterParams::FilterType::Bandpass},
            {"bandstop", AnalogFilterParams::FilterType::Bandstop}};
    registerEnumParameter<AnalogFilterParams, AnalogFilterParams::FilterType>(
            "Analog Filter", "filter_type", &AnalogFilterParams::filter_type, filter_type_map);

    registerBasicParameter<AnalogFilterParams, double>(
            "Analog Filter", "cutoff_frequency", &AnalogFilterParams::cutoff_frequency);

    registerBasicParameter<AnalogFilterParams, double>(
            "Analog Filter", "cutoff_frequency2", &AnalogFilterParams::cutoff_frequency2);

    registerBasicParameter<AnalogFilterParams, int>(
            "Analog Filter", "order", &AnalogFilterParams::order);

    registerBasicParameter<AnalogFilterParams, double>(
            "Analog Filter", "ripple", &AnalogFilterParams::ripple);

    registerBasicParameter<AnalogFilterParams, bool>(
            "Analog Filter", "zero_phase", &AnalogFilterParams::zero_phase);

    registerBasicParameter<AnalogFilterParams, double>(
            "Analog Filter", "sampling_rate", &AnalogFilterParams::sampling_rate);

    // Register Hilbert Phase parameters
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "low_frequency", &HilbertPhaseParams::lowFrequency);
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "high_frequency", &HilbertPhaseParams::highFrequency);
    registerBasicParameter<HilbertPhaseParams, size_t>(
            "Hilbert Phase", "discontinuity_threshold", &HilbertPhaseParams::discontinuityThreshold);

    std::cout << "Parameter factory initialized with default setters" << std::endl;
}
