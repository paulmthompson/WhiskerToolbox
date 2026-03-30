/**
 * @file ProcessingStep.hpp
 * @brief Descriptor for a named image processing step in the pipeline
 */

#ifndef MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_HPP
#define MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_HPP

#include "ParameterSchema/ParameterSchema.hpp"

#include <functional>
#include <nlohmann/json.hpp>
#include <opencv2/core/mat.hpp>
#include <string>

namespace WhiskerToolbox::MediaProcessing {

/**
 * @brief Descriptor for a single image processing step
 *
 * Each step has a display name (for the UI), a chain key (used in
 * MediaData::addProcessingStep), an execution order (embedded in the
 * chain key prefix), a ParameterSchema for auto-generating the UI,
 * and a type-erased apply function that takes a cv::Mat and JSON params.
 */
struct ProcessingStep {
    std::string display_name;///< Human-readable name shown in UI sections
    std::string chain_key;   ///< Key used with MediaData processing chain (e.g. "1__lineartransform")

    /// Schema describing the parameters (for AutoParamWidget)
    ParameterSchema schema;

    /// Type-erased apply function: applies this step to an image given JSON-serialized params
    std::function<void(cv::Mat &, nlohmann::json const &)> apply;
};

}// namespace WhiskerToolbox::MediaProcessing

#endif// MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_HPP
