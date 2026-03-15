/**
 * @file ProcessingStepRegistry.cpp
 * @brief Implementation of ProcessingStepRegistry and static step registrations
 */

#include "ProcessingStepRegistry.hpp"

#include "ProcessingOptionsSchema.hpp"

#include "ImageProcessing/OpenCVUtility.hpp"

namespace WhiskerToolbox::MediaProcessing {

ProcessingStepRegistry & ProcessingStepRegistry::instance() {
    static ProcessingStepRegistry registry;
    return registry;
}

void ProcessingStepRegistry::registerStep(ProcessingStep step) {
    _steps.push_back(std::move(step));
}

ProcessingStep const * ProcessingStepRegistry::findByKey(std::string const & chain_key) const {
    for (auto const & s: _steps) {
        if (s.chain_key == chain_key) return &s;
    }
    return nullptr;
}

// --- Static registrations ---

static RegisterStep<ContrastOptions> const reg_contrast{
        "Linear Transform",
        "1__lineartransform",
        [](cv::Mat & m, ContrastOptions const & o) { ImageProcessing::linear_transform(m, o); }};

static RegisterStep<GammaOptions> const reg_gamma{
        "Gamma Correction",
        "2__gamma",
        [](cv::Mat & m, GammaOptions const & o) { ImageProcessing::gamma_transform(m, o); }};

static RegisterStep<SharpenOptions> const reg_sharpen{
        "Image Sharpening",
        "3__sharpen",
        [](cv::Mat & m, SharpenOptions const & o) { ImageProcessing::sharpen_image(m, o); }};

static RegisterStep<ClaheOptions> const reg_clahe{
        "CLAHE",
        "4__clahe",
        [](cv::Mat & m, ClaheOptions const & o) { ImageProcessing::clahe(m, o); }};

static RegisterStep<BilateralOptions> const reg_bilateral{
        "Bilateral Filter",
        "5__bilateral",
        [](cv::Mat & m, BilateralOptions const & o) { ImageProcessing::bilateral_filter(m, o); }};

static RegisterStep<MedianOptions> const reg_median{
        "Median Filter",
        "6__median",
        [](cv::Mat & m, MedianOptions const & o) { ImageProcessing::median_filter(m, o); }};

}// namespace WhiskerToolbox::MediaProcessing
