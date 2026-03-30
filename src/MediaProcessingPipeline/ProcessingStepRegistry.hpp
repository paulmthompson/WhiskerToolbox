/**
 * @file ProcessingStepRegistry.hpp
 * @brief Singleton registry of available image processing steps
 */

#ifndef MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_REGISTRY_HPP
#define MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_REGISTRY_HPP

#include "ProcessingStep.hpp"

#include <rfl/json.hpp>
#include <string>
#include <vector>

namespace WhiskerToolbox::MediaProcessing {

/**
 * @brief Singleton registry mapping step names to ProcessingStep descriptors
 *
 * Processing steps register themselves at static-initialization time
 * via the RegisterStep RAII helper. The registry is ordered by insertion
 * order (which mirrors execution order when steps are registered in
 * a single translation unit).
 */
class ProcessingStepRegistry {
public:
    static ProcessingStepRegistry & instance();

    void registerStep(ProcessingStep step);

    [[nodiscard]] std::vector<ProcessingStep> const & steps() const { return _steps; }

    [[nodiscard]] ProcessingStep const * findByKey(std::string const & chain_key) const;

private:
    ProcessingStepRegistry() = default;
    std::vector<ProcessingStep> _steps;
};

/**
 * @brief RAII helper for static registration of a processing step
 *
 * Usage:
 * @code
 * static RegisterStep<ContrastOptions> reg_contrast{
 *     "Linear Transform",
 *     "1__lineartransform",
 *     [](cv::Mat& m, ContrastOptions const& o) { ImageProcessing::linear_transform(m, o); }
 * };
 * @endcode
 */
template<typename Params>
struct RegisterStep {
    RegisterStep(
            std::string display_name,
            std::string chain_key,
            std::function<void(cv::Mat &, Params const &)> typed_apply) {
        auto schema = extractParameterSchema<Params>();

        ProcessingStep step;
        step.display_name = std::move(display_name);
        step.chain_key = std::move(chain_key);
        step.schema = std::move(schema);
        step.apply = [fn = std::move(typed_apply)](cv::Mat & mat, nlohmann::json const & params_json) {
            auto const result = rfl::json::read<Params>(params_json.dump());
            if (result) {
                fn(mat, result.value());
            }
        };

        ProcessingStepRegistry::instance().registerStep(std::move(step));
    }
};

}// namespace WhiskerToolbox::MediaProcessing

#endif// MEDIA_PROCESSING_PIPELINE_PROCESSING_STEP_REGISTRY_HPP
