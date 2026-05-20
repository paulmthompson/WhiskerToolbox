#include "line_resample.hpp"

#include "CoreGeometry/line_resampling.hpp"
#include "CoreMath/parametric_polynomial_utils.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iostream>
#include <map>

namespace {

[[nodiscard]] char const * algorithm_name(LineSimplificationAlgorithm algorithm) {
    switch (algorithm) {
        case LineSimplificationAlgorithm::FixedSpacing:
            return "FixedSpacing";
        case LineSimplificationAlgorithm::DouglasPeucker:
            return "DouglasPeucker";
        case LineSimplificationAlgorithm::PolynomialSmooth:
            return "PolynomialSmooth";
    }
    return "Unknown";
}

[[nodiscard]] int clamp_polynomial_order(int order) {
    return std::max(1, std::min(order, 9));
}

/**
 * @brief Smooth a line with parametric polynomial fitting and resample at target spacing
 */
[[nodiscard]] Line2D smooth_line_polynomial(Line2D const & line, int order, float target_spacing) {
    if (line.size() <= static_cast<size_t>(order)) {
        return line;
    }

    ParametricCoefficients const coeffs = fit_parametric_polynomials(line, order);
    if (coeffs.success) {
        return generate_smoothed_line(line, coeffs.x_coeffs, coeffs.y_coeffs, order, target_spacing);
    }
    return resample_line_points(line, target_spacing);
}

[[nodiscard]] Line2D resample_single_line(Line2D const & line, LineResampleParameters const & params) {
    if (line.empty()) {
        return line;
    }

    switch (params.algorithm) {
        case LineSimplificationAlgorithm::FixedSpacing:
            if (line.size() <= 2) {
                return line;
            }
            return resample_line_points(line, params.target_spacing);
        case LineSimplificationAlgorithm::DouglasPeucker:
            if (line.size() <= 2) {
                return line;
            }
            return douglas_peucker_simplify(line, params.epsilon);
        case LineSimplificationAlgorithm::PolynomialSmooth:
            if (line.size() <= 2) {
                return line;
            }
            return smooth_line_polynomial(
                    line,
                    clamp_polynomial_order(params.polynomial_order),
                    params.target_spacing);
    }
    return line;
}

} // namespace

std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params) {
    return line_resample(line_data, params, nullptr);
}

std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params,
        ProgressCallback progressCallback) {
    auto result_line_data = std::make_shared<LineData>();

    if (!line_data) {
        std::cerr << "Error: line_data is null." << std::endl;
        return result_line_data;
    }

    spdlog::debug(
            "LineResampleOperation: algorithm={}, target_spacing={}, epsilon={}, polynomial_order={}",
            algorithm_name(params.algorithm),
            params.target_spacing,
            params.epsilon,
            params.polynomial_order);

    auto resampled_line_map = std::map<TimeFrameIndex, std::vector<Line2D>>();
    int const total_lines = line_data->getTotalEntryCount();
    if (total_lines == 0) {
        if (progressCallback) {
            progressCallback(100);
        }
        result_line_data->setImageSize(line_data->getImageSize());
        return result_line_data;
    }

    int processed_lines = 0;
    if (progressCallback) {
        progressCallback(0);
    }

    for (auto const & [time, entity_id, line]: line_data->flattened_data()) {

        Line2D new_line_at_time;
        if (!line.empty()) {
            new_line_at_time = resample_single_line(line, params);
        }
        processed_lines++;
        if (progressCallback && total_lines > 0) {
            int const current_progress =
                    static_cast<int>((static_cast<double>(processed_lines) / total_lines) * 100.0);
            progressCallback(current_progress);
        }

        if (!line_data->getAtTime(time).empty() || !new_line_at_time.empty()) {
            resampled_line_map[time].push_back(std::move(new_line_at_time));
        }
    }

    result_line_data = std::make_shared<LineData>(resampled_line_map);
    result_line_data->setImageSize(line_data->getImageSize());

    if (progressCallback) {
        progressCallback(100);
    }
    spdlog::debug("LineResampleOperation executed successfully.");
    return result_line_data;
}

///////////////////////////////////////////////////////////////////////////////

std::string LineResampleOperation::getName() const {
    return "Resample Line";
}

std::type_index LineResampleOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineResampleOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineResampleOperation::getDefaultParameters() const {
    return std::make_unique<LineResampleParameters>();
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters,
                                               ProgressCallback progressCallback) {
    auto const * line_data_sptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!line_data_sptr_ptr || !(*line_data_sptr_ptr)) {
        std::cerr << "LineResampleOperation::execute called with incompatible variant type or null data." << std::endl;
        if (progressCallback) {
            progressCallback(100);
        }
        return {};
    }
    LineData const * input_line_data = (*line_data_sptr_ptr).get();

    LineResampleParameters currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams = dynamic_cast<LineResampleParameters const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "Warning: LineResampleOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
        }
    }

    std::shared_ptr<LineData> result_line_data =
            line_resample(input_line_data, currentParams, progressCallback);

    if (!result_line_data) {
        std::cerr << "LineResampleOperation::execute: 'line_resample' failed to produce a result." << std::endl;
        if (progressCallback) {
            progressCallback(100);
        }
        return {};
    }

    if (progressCallback) {
        progressCallback(100);
    }
    return result_line_data;
}
