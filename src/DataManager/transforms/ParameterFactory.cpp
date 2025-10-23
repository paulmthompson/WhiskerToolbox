#include "ParameterFactory.hpp"

#include "AnalogTimeSeries/AnalogFilter/analog_filter.hpp"
#include "AnalogTimeSeries/AnalogHilbertPhase/analog_hilbert_phase.hpp"
#include "AnalogTimeSeries/Analog_Event_Threshold/analog_event_threshold.hpp"
#include "AnalogTimeSeries/Analog_Interval_Threshold/analog_interval_threshold.hpp"
#include "AnalogTimeSeries/Analog_Scaling/analog_scaling.hpp"
#include "DigitalIntervalSeries/Digital_Interval_Group/digital_interval_group.hpp"
#include "Lines/Line_Alignment/line_alignment.hpp"
#include "Lines/Line_Angle/line_angle.hpp"
#include "Lines/Line_Clip/line_clip.hpp"
#include "Lines/Line_Curvature/line_curvature.hpp"
#include "Lines/Line_Kalman_Grouping/line_kalman_grouping.hpp"
#include "Lines/Line_Min_Point_Dist/line_min_point_dist.hpp"
#include "Lines/Line_Point_Extraction/line_point_extraction.hpp"
#include "Lines/Line_Proximity_Grouping/line_proximity_grouping.hpp"
#include "Lines/Line_Resample/line_resample.hpp"
#include "Lines/Line_Subsegment/line_subsegment.hpp"
#include "Masks/Mask_Connected_Component/mask_connected_component.hpp"
#include "Masks/Mask_Median_Filter/mask_median_filter.hpp"
#include "Masks/Mask_Principal_Axis/mask_principal_axis.hpp"
#include "Masks/Mask_To_Line/mask_to_line.hpp"
#include "Media/whisker_tracing.hpp"

#include <iostream>

void ParameterFactory::registerParameterSetter(std::string const & transform_name,// NOLINT(bugprone-easily-swappable-parameters)
                                               std::string const & param_name,    // NOLINT(bugprone-easily-swappable-parameters)
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

    // ==================================================
    // =============== Analog Time Series ===============
    // ==================================================

    // =============== Threshold Event Detection ===============

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

    // ============== Analog Interval Threshold ==============

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

    registerBasicParameter<IntervalThresholdParams, double>(
            "Threshold Interval Detection", "threshold_value", &IntervalThresholdParams::thresholdValue);

    std::unordered_map<std::string, IntervalThresholdParams::MissingDataMode> missing_data_mode_map = {
            {"Zero", IntervalThresholdParams::MissingDataMode::TREAT_AS_ZERO},
            {"Ignore", IntervalThresholdParams::MissingDataMode::IGNORE}};
    registerEnumParameter<IntervalThresholdParams, IntervalThresholdParams::MissingDataMode>(
            "Threshold Interval Detection", "missing_data_mode", &IntervalThresholdParams::missingDataMode, missing_data_mode_map);

    // ================== Analog Hilbert Phase ==================

    registerBasicParameter<HilbertPhaseParams, size_t>(
            "Hilbert Phase", "discontinuity_threshold", &HilbertPhaseParams::discontinuityThreshold);

    std::unordered_map<std::string, HilbertPhaseParams::OutputType> hilbert_output_type_map = {
            {"Phase", HilbertPhaseParams::OutputType::Phase},
            {"Amplitude", HilbertPhaseParams::OutputType::Amplitude}};
    registerEnumParameter<HilbertPhaseParams, HilbertPhaseParams::OutputType>(
            "Hilbert Phase", "output_type", &HilbertPhaseParams::outputType, hilbert_output_type_map);

    // Windowed processing parameters for long signals
    registerBasicParameter<HilbertPhaseParams, size_t>(
            "Hilbert Phase", "max_chunk_size", &HilbertPhaseParams::maxChunkSize);
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "overlap_fraction", &HilbertPhaseParams::overlapFraction);
    registerBasicParameter<HilbertPhaseParams, bool>(
            "Hilbert Phase", "use_windowing", &HilbertPhaseParams::useWindowing);

    // Bandpass filtering parameters
    registerBasicParameter<HilbertPhaseParams, bool>(
            "Hilbert Phase", "apply_bandpass_filter", &HilbertPhaseParams::applyBandpassFilter);
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "filter_low_freq", &HilbertPhaseParams::filterLowFreq);
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "filter_high_freq", &HilbertPhaseParams::filterHighFreq);
    registerBasicParameter<HilbertPhaseParams, int>(
            "Hilbert Phase", "filter_order", &HilbertPhaseParams::filterOrder);
    registerBasicParameter<HilbertPhaseParams, double>(
            "Hilbert Phase", "sampling_rate", &HilbertPhaseParams::samplingRate);

    // ================== Analog Scaling ==================

    std::unordered_map<std::string, ScalingMethod> scaling_method_map = {
            {"FixedGain", ScalingMethod::FixedGain},                // Multiply by constant factor
            {"ZScore", ScalingMethod::ZScore},                      // (x - mean) / std
            {"StandardDeviation", ScalingMethod::StandardDeviation},// Scale so X std devs = 1.0
            {"MinMax", ScalingMethod::MinMax},                      // Scale to [0, 1] range
            {"RobustScaling", ScalingMethod::RobustScaling},        // (x - median) / IQR
            {"UnitVariance", ScalingMethod::UnitVariance},          // Scale to unit variance (std = 1)
            {"Centering", ScalingMethod::Centering}                 // Subtract mean (center around 0)
    };
    registerEnumParameter<AnalogScalingParams, ScalingMethod>(
            "Scale and Normalize", "method", &AnalogScalingParams::method, scaling_method_map);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "gain_factor", &AnalogScalingParams::gain_factor);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "std_dev_target", &AnalogScalingParams::std_dev_target);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "min_target", &AnalogScalingParams::min_target);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "max_target", &AnalogScalingParams::max_target);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "quantile_low", &AnalogScalingParams::quantile_low);

    registerBasicParameter<AnalogScalingParams, double>(
            "Scale and Normalize", "quantile_high", &AnalogScalingParams::quantile_high);

    // ====================================================
    // ============== Digital Interval Series =============
    // ====================================================

    // ================= Digital Interval Group ===============

    registerBasicParameter<GroupParams, double>(
            "Group Intervals", "max_spacing", &GroupParams::maxSpacing);

    // ====================================================
    // ============== Line Series ==================
    // ====================================================

    // ================= Line Alignment ===============

    registerDataParameter<LineAlignmentParameters, MediaData>(
            "Line Alignment to Bright Features", "media_data", &LineAlignmentParameters::media_data);

    registerBasicParameter<LineAlignmentParameters, int>(
            "Line Alignment to Bright Features", "width", &LineAlignmentParameters::width);

    registerBasicParameter<LineAlignmentParameters, int>(
            "Line Alignment to Bright Features", "perpendicular_range", &LineAlignmentParameters::perpendicular_range);

    registerBasicParameter<LineAlignmentParameters, bool>(
            "Line Alignment to Bright Features", "use_processed_data", &LineAlignmentParameters::use_processed_data);

    std::unordered_map<std::string, FWHMApproach> fwhm_approach_map = {
            {"PEAK_WIDTH_HALF_MAX", FWHMApproach::PEAK_WIDTH_HALF_MAX},
            {"GAUSSIAN_FIT", FWHMApproach::GAUSSIAN_FIT},
            {"THRESHOLD_BASED", FWHMApproach::THRESHOLD_BASED}};
    registerEnumParameter<LineAlignmentParameters, FWHMApproach>(
            "Line Alignment to Bright Features", "approach", &LineAlignmentParameters::approach, fwhm_approach_map);

    std::unordered_map<std::string, LineAlignmentOutputMode> line_alignment_output_mode_map = {
            {"ALIGNED_VERTICES", LineAlignmentOutputMode::ALIGNED_VERTICES},
            {"FWHM_PROFILE_EXTENTS", LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS}};
    registerEnumParameter<LineAlignmentParameters, LineAlignmentOutputMode>(
            "Line Alignment to Bright Features", "output_mode", &LineAlignmentParameters::output_mode, line_alignment_output_mode_map);


    // ==================== Line Angle ===============

    registerBasicParameter<LineAngleParameters, float>(
            "Calculate Line Angle", "position", &LineAngleParameters::position);

    std::unordered_map<std::string, AngleCalculationMethod> angle_calculation_method_map = {
            {"Direct Points", AngleCalculationMethod::DirectPoints},
            {"Polynomial Fit", AngleCalculationMethod::PolynomialFit}};

    registerEnumParameter<LineAngleParameters, AngleCalculationMethod>(
            "Calculate Line Angle", "method", &LineAngleParameters::method, angle_calculation_method_map);

    registerBasicParameter<LineAngleParameters, int>(
            "Calculate Line Angle", "polynomial_order", &LineAngleParameters::polynomial_order);

    registerBasicParameter<LineAngleParameters, float>(
            "Calculate Line Angle", "reference_x", &LineAngleParameters::reference_x);

    registerBasicParameter<LineAngleParameters, float>(
            "Calculate Line Angle", "reference_y", &LineAngleParameters::reference_y);

    // ==================== Line Clip ===============

    std::unordered_map<std::string, ClipSide> clip_side_map = {
            {"KeepBase", ClipSide::KeepBase},   // Keep the portion from line start to intersection
            {"KeepDistal", ClipSide::KeepDistal}// Keep the portion from intersection to line end
    };
    registerEnumParameter<LineClipParameters, ClipSide>(
            "Clip Line by Reference Line", "clip_side", &LineClipParameters::clip_side, clip_side_map);

    registerDataParameter<LineClipParameters, LineData>(
            "Clip Line by Reference Line", "reference_line_data", &LineClipParameters::reference_line_data);

    registerBasicParameter<LineClipParameters, int>(
            "Clip Line by Reference Line", "reference_frame", &LineClipParameters::reference_frame);


    // ==================== Line Curvature ===============

    registerBasicParameter<LineCurvatureParameters, float>(
            "Calculate Line Curvature", "position", &LineCurvatureParameters::position);

    std::unordered_map<std::string, CurvatureCalculationMethod> curvature_calculation_method_map = {
            {"PolynomialFit", CurvatureCalculationMethod::PolynomialFit}// Only method for now
    };
    registerEnumParameter<LineCurvatureParameters, CurvatureCalculationMethod>(
            "Calculate Line Curvature", "method", &LineCurvatureParameters::method, curvature_calculation_method_map);

    registerBasicParameter<LineCurvatureParameters, int>(
            "Calculate Line Curvature", "polynomial_order", &LineCurvatureParameters::polynomial_order);

    registerBasicParameter<LineCurvatureParameters, float>(
            "Calculate Line Curvature", "fitting_window_percentage", &LineCurvatureParameters::fitting_window_percentage);

    // ==================== Line Min Point Dist ===============

    registerDataParameter<LineMinPointDistParameters, PointData>(
            "Calculate Line to Point Distance", "point_data", &LineMinPointDistParameters::point_data);

    // ==================== Line Point Extraction ===============

    std::unordered_map<std::string, PointExtractionMethod> point_extraction_method_map = {
            {"Direct", PointExtractionMethod::Direct},       // Direct point selection based on indices
            {"Parametric", PointExtractionMethod::Parametric}// Use parametric polynomial interpolation
    };
    registerEnumParameter<LinePointExtractionParameters, PointExtractionMethod>(
            "Extract Point from Line", "method", &LinePointExtractionParameters::method, point_extraction_method_map);

    registerBasicParameter<LinePointExtractionParameters, float>(
            "Extract Point from Line", "position", &LinePointExtractionParameters::position);

    registerBasicParameter<LinePointExtractionParameters, int>(
            "Extract Point from Line", "polynomial_order", &LinePointExtractionParameters::polynomial_order);

    registerBasicParameter<LinePointExtractionParameters, bool>(
            "Extract Point from Line", "use_interpolation", &LinePointExtractionParameters::use_interpolation);

    // ==================== Line Resample ===============

    std::unordered_map<std::string, LineSimplificationAlgorithm> line_simplification_map = {
            {"Fixed Spacing", LineSimplificationAlgorithm::FixedSpacing},
            {"Douglas-Peucker", LineSimplificationAlgorithm::DouglasPeucker}};
    registerEnumParameter<LineResampleParameters, LineSimplificationAlgorithm>(
            "Resample Line", "algorithm", &LineResampleParameters::algorithm, line_simplification_map);

    registerBasicParameter<LineResampleParameters, float>(
            "Resample Line", "target_spacing", &LineResampleParameters::target_spacing);

    registerBasicParameter<LineResampleParameters, float>(
            "Resample Line", "epsilon", &LineResampleParameters::epsilon);

    // ==================== Line Subsegment ===============

    std::unordered_map<std::string, SubsegmentExtractionMethod> subsegment_extraction_method_map = {
            {"Direct", SubsegmentExtractionMethod::Direct},       // Direct point extraction based on indices
            {"Parametric", SubsegmentExtractionMethod::Parametric}// Use parametric polynomial interpolation
    };
    registerEnumParameter<LineSubsegmentParameters, SubsegmentExtractionMethod>(
            "Extract Line Subsegment", "method", &LineSubsegmentParameters::method, subsegment_extraction_method_map);

    registerBasicParameter<LineSubsegmentParameters, float>(
            "Extract Line Subsegment", "start_position", &LineSubsegmentParameters::start_position);

    registerBasicParameter<LineSubsegmentParameters, float>(
            "Extract Line Subsegment", "end_position", &LineSubsegmentParameters::end_position);

    registerBasicParameter<LineSubsegmentParameters, int>(
            "Extract Line Subsegment", "polynomial_order", &LineSubsegmentParameters::polynomial_order);

    registerBasicParameter<LineSubsegmentParameters, int>(
            "Extract Line Subsegment", "output_points", &LineSubsegmentParameters::output_points);

    registerBasicParameter<LineSubsegmentParameters, bool>(
            "Extract Line Subsegment", "preserve_original_spacing", &LineSubsegmentParameters::preserve_original_spacing);

    // ====================================================
    // ============== Mask Series ==================
    // ====================================================

    // ==================== Mask Area ===============
    // No parameters needed for mask area calculation

    // ==================== Mask Centroid ===============
    // No parameters needed for mask centroid calculation

    // ==================== Mask Connected Component ===============
    registerBasicParameter<MaskConnectedComponentParameters, int>(
            "Remove Small Connected Components", "threshold", &MaskConnectedComponentParameters::threshold);

    // ==================== Mask Hole Filling ===============
    // No parameters needed for mask hole filling calculation

    // ==================== Mask Median Filter ===============
    registerBasicParameter<MaskMedianFilterParameters, int>(
            "Apply Median Filter", "window_size", &MaskMedianFilterParameters::window_size);

    // ==================== Mask Principal Axis ===============
    std::unordered_map<std::string, PrincipalAxisType> principal_axis_type_map = {
            {"Major", PrincipalAxisType::Major},
            {"Minor", PrincipalAxisType::Minor}};

    registerEnumParameter<MaskPrincipalAxisParameters, PrincipalAxisType>(
            "Calculate Mask Principal Axis", "axis_type", &MaskPrincipalAxisParameters::axis_type, principal_axis_type_map);


    // ==================== Mask Skeletonize ===============
    // No parameters needed for mask skeletonize calculation

    // ==================== Mask To Line ===============

    std::unordered_map<std::string, LinePointSelectionMethod> line_point_selection_method_map = {
            {"NearestToReference", LinePointSelectionMethod::NearestToReference},
            {"Skeletonize", LinePointSelectionMethod::Skeletonize}};
    registerEnumParameter<MaskToLineParameters, LinePointSelectionMethod>(
            "Convert Mask To Line", "method", &MaskToLineParameters::method, line_point_selection_method_map);

    registerBasicParameter<MaskToLineParameters, float>(
            "Convert Mask To Line", "reference_x", &MaskToLineParameters::reference_x);

    registerBasicParameter<MaskToLineParameters, float>(
            "Convert Mask To Line", "reference_y", &MaskToLineParameters::reference_y);

    registerBasicParameter<MaskToLineParameters, int>(
            "Convert Mask To Line", "polynomial_order", &MaskToLineParameters::polynomial_order);

    registerBasicParameter<MaskToLineParameters, float>(
            "Convert Mask To Line", "error_threshold", &MaskToLineParameters::error_threshold);

    registerBasicParameter<MaskToLineParameters, bool>(
            "Convert Mask To Line", "remove_outliers", &MaskToLineParameters::remove_outliers);

    registerBasicParameter<MaskToLineParameters, int>(
            "Convert Mask To Line", "input_point_subsample_factor", &MaskToLineParameters::input_point_subsample_factor);

    registerBasicParameter<MaskToLineParameters, bool>(
            "Convert Mask To Line", "should_smooth_line", &MaskToLineParameters::should_smooth_line);

    registerBasicParameter<MaskToLineParameters, float>(
            "Convert Mask To Line", "output_resolution", &MaskToLineParameters::output_resolution);

    // ====================================================
    // ============== Media Series ==================
    // ====================================================

    // ==================== Whisker Tracing ===============

    registerBasicParameter<WhiskerTracingParameters, bool>(
            "Whisker Tracing", "use_processed_data", &WhiskerTracingParameters::use_processed_data);

    registerBasicParameter<WhiskerTracingParameters, int>(
            "Whisker Tracing", "clip_length", &WhiskerTracingParameters::clip_length);

    registerBasicParameter<WhiskerTracingParameters, float>(
            "Whisker Tracing", "whisker_length_threshold", &WhiskerTracingParameters::whisker_length_threshold);

    registerBasicParameter<WhiskerTracingParameters, int>(
            "Whisker Tracing", "batch_size", &WhiskerTracingParameters::batch_size);

    registerBasicParameter<WhiskerTracingParameters, bool>(
            "Whisker Tracing", "use_parallel_processing", &WhiskerTracingParameters::use_parallel_processing);

    registerBasicParameter<WhiskerTracingParameters, bool>(
            "Whisker Tracing", "use_mask_data", &WhiskerTracingParameters::use_mask_data);

    registerDataParameter<WhiskerTracingParameters, MaskData>(
            "Whisker Tracing", "mask_data", &WhiskerTracingParameters::mask_data);

    // ====================================================
    // ============== Grouping Operations =================
    // ====================================================

    // ==================== Line Proximity Grouping ===============
    registerBasicParameter<LineProximityGroupingParameters, float>(
            "Group Lines by Proximity", "distance_threshold", &LineProximityGroupingParameters::distance_threshold);

    registerBasicParameter<LineProximityGroupingParameters, float>(
            "Group Lines by Proximity", "position_along_line", &LineProximityGroupingParameters::position_along_line);

    registerBasicParameter<LineProximityGroupingParameters, bool>(
            "Group Lines by Proximity", "create_new_group_for_outliers", &LineProximityGroupingParameters::create_new_group_for_outliers);

    registerBasicParameter<LineProximityGroupingParameters, std::string>(
            "Group Lines by Proximity", "new_group_name", &LineProximityGroupingParameters::new_group_name);

    // ==================== Line Kalman Grouping ===============
    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "dt", &LineKalmanGroupingParameters::dt);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "process_noise_position", &LineKalmanGroupingParameters::process_noise_position);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "process_noise_velocity", &LineKalmanGroupingParameters::process_noise_velocity);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "static_feature_process_noise_scale", &LineKalmanGroupingParameters::static_feature_process_noise_scale);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "measurement_noise_position", &LineKalmanGroupingParameters::measurement_noise_position);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "measurement_noise_length", &LineKalmanGroupingParameters::measurement_noise_length);

    registerBasicParameter<LineKalmanGroupingParameters, bool>(
            "Group Lines using Kalman Filtering", "auto_estimate_static_noise", &LineKalmanGroupingParameters::auto_estimate_static_noise);

    registerBasicParameter<LineKalmanGroupingParameters, bool>(
            "Group Lines using Kalman Filtering", "auto_estimate_measurement_noise", &LineKalmanGroupingParameters::auto_estimate_measurement_noise);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "static_noise_percentile", &LineKalmanGroupingParameters::static_noise_percentile);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "initial_position_uncertainty", &LineKalmanGroupingParameters::initial_position_uncertainty);

    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "initial_velocity_uncertainty", &LineKalmanGroupingParameters::initial_velocity_uncertainty);

    registerBasicParameter<LineKalmanGroupingParameters, bool>(
            "Group Lines using Kalman Filtering", "verbose_output", &LineKalmanGroupingParameters::verbose_output);

    // New: cheap linkage threshold exposed to UI
    registerBasicParameter<LineKalmanGroupingParameters, double>(
            "Group Lines using Kalman Filtering", "cheap_assignment_threshold", &LineKalmanGroupingParameters::cheap_assignment_threshold);

    // New: putative output control
    registerBasicParameter<LineKalmanGroupingParameters, bool>(
            "Group Lines using Kalman Filtering", "write_to_putative_groups", &LineKalmanGroupingParameters::write_to_putative_groups);
    registerBasicParameter<LineKalmanGroupingParameters, std::string>(
            "Group Lines using Kalman Filtering", "putative_group_prefix", &LineKalmanGroupingParameters::putative_group_prefix);
}
