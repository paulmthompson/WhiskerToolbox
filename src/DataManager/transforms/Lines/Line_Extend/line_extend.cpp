/// @file line_extend.cpp
/// @brief V1 transform that extends lines to a reference polyline.

#include "line_extend.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <cmath>
#include <iostream>
#include <vector>

bool validateLineExtendImageSizes(
        LineData const * input_line_data,
        LineData const * reference_line_data) {

    if (!input_line_data || !reference_line_data) {
        return false;
    }

    ImageSize const input_size = input_line_data->getImageSize();
    ImageSize const reference_size = reference_line_data->getImageSize();

    if (!input_size.isDefined()) {
        std::cerr << "LineExtend: Input LineData image size is undefined." << std::endl;
        return false;
    }

    if (!reference_size.isDefined()) {
        std::cerr << "LineExtend: Reference LineData image size is undefined." << std::endl;
        return false;
    }

    if (input_size != reference_size) {
        std::cerr << "LineExtend: Image size mismatch. Input is "
                  << input_size.width << "x" << input_size.height << ", reference is "
                  << reference_size.width << "x" << reference_size.height << "." << std::endl;
        return false;
    }

    return true;
}

std::shared_ptr<LineData> extend_lines(
        LineData const * line_data,
        LineExtendParameters const * params) {
    return extend_lines(line_data, params, [](int) {});
}

std::shared_ptr<LineData> extend_lines(
        LineData const * line_data,
        LineExtendParameters const * params,
        ProgressCallback const & progressCallback) {

    auto result_line_data = std::make_shared<LineData>();

    if (!line_data || !params || !params->reference_line_data) {
        std::cerr << "LineExtend: Invalid input parameters." << std::endl;
        progressCallback(100);
        return result_line_data;
    }

    if (!validateLineExtendImageSizes(line_data, params->reference_line_data.get())) {
        progressCallback(100);
        return result_line_data;
    }

    result_line_data->setImageSize(line_data->getImageSize());

    auto const & reference_lines =
            params->reference_line_data->getAtTime(TimeFrameIndex(params->reference_frame));
    if (reference_lines.empty()) {
        std::cerr << "LineExtend: No reference line found at frame " << params->reference_frame << std::endl;
        progressCallback(100);
        return result_line_data;
    }

    Line2D const & reference_line = reference_lines[0];

    auto times_with_data = line_data->getTimesWithData();
    if (times_with_data.empty()) {
        progressCallback(100);
        return result_line_data;
    }

    progressCallback(0);

    size_t processed_times = 0;

    for (auto time: times_with_data) {
        auto const & lines_at_time = line_data->getAtTime(time);

        for (auto const & line: lines_at_time) {
            if (line.size() < 2) {
                continue;
            }

            Line2D const extended_line = extend_line_at_reference(
                    line,
                    reference_line,
                    params->extend_end,
                    params->tangent_distance_pixels);

            if (extended_line.size() >= 2) {
                result_line_data->addAtTime(time, extended_line, NotifyObservers::No);
            }
        }

        processed_times++;
        int const progress = static_cast<int>(
                std::round(static_cast<double>(processed_times) / static_cast<double>(times_with_data.size()) * 100.0));
        progressCallback(progress);
    }

    progressCallback(100);
    return result_line_data;
}

std::string LineExtendOperation::getName() const {
    return "Extend Line by Reference Line";
}

std::type_index LineExtendOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineExtendOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineExtendOperation::getDefaultParameters() const {
    return std::make_unique<LineExtendParameters>();
}

DataTypeVariant LineExtendOperation::execute(DataTypeVariant const & dataVariant,
                                             TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineExtendOperation::execute(DataTypeVariant const & dataVariant,
                                             TransformParametersBase const * transformParameters,
                                             ProgressCallback progressCallback) {

    auto const * line_data_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);

    if (!line_data_ptr || !(*line_data_ptr)) {
        std::cerr << "LineExtendOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }

    LineData const * input_line_data = (*line_data_ptr).get();

    LineExtendParameters const * params = nullptr;
    std::unique_ptr<TransformParametersBase> default_params_owner;

    if (transformParameters) {
        params = dynamic_cast<LineExtendParameters const *>(transformParameters);
        if (!params) {
            std::cerr << "LineExtendOperation::execute: Invalid parameter type. Using defaults." << std::endl;
            default_params_owner = getDefaultParameters();
            params = dynamic_cast<LineExtendParameters const *>(default_params_owner.get());
        }
    } else {
        default_params_owner = getDefaultParameters();
        params = dynamic_cast<LineExtendParameters const *>(default_params_owner.get());
    }

    if (!params) {
        std::cerr << "LineExtendOperation::execute: Failed to get parameters." << std::endl;
        return {};
    }

    std::shared_ptr<LineData> result = extend_lines(input_line_data, params, progressCallback);

    if (!result) {
        std::cerr << "LineExtendOperation::execute: 'extend_lines' failed to produce a result." << std::endl;
        return {};
    }

    return result;
}
