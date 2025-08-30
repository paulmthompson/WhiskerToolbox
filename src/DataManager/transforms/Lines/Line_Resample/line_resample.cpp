#include "line_resample.hpp"

#include "CoreGeometry/line_resampling.hpp"// For the actual resampling
#include "Lines/Line_Data.hpp"

#include <iostream>// For error messages / cout
#include <map>     // for std::map

std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params) {
    // Call the version with a null progress callback
    return line_resample(line_data, params, nullptr);
}

/**
 * @brief Resamples lines in a LineData object based on the specified algorithm, with progress reporting.
 *
 * This function processes all lines in the input LineData object using the specified algorithm
 * (FixedSpacing or Douglas-Peucker) and returns a new LineData object with the resampled lines.
 *
 * @param line_data A pointer to the input LineData object. Must not be null.
 * @param params A struct containing the algorithm type, target spacing (for FixedSpacing),
 *               and epsilon value (for Douglas-Peucker).
 * @param progressCallback An optional function that can be called to report progress (0-100).
 *                         If nullptr, progress is not reported.
 * @return A std::shared_ptr<LineData> containing the resampled lines.
 *         Returns an empty LineData if line_data is null or has no data.
 */
std::shared_ptr<LineData> line_resample(
        LineData const * line_data,
        LineResampleParameters const & params,
        ProgressCallback progressCallback) {
    auto result_line_data = std::make_shared<LineData>();

    if (!line_data) {
        std::cerr << "Error: line_data is null." << std::endl;
        return result_line_data;
    }

    std::cout << "LineResampleOperation: algorithm = "
              << (params.algorithm == LineSimplificationAlgorithm::FixedSpacing ? "FixedSpacing" : "DouglasPeucker")
              << ", target_spacing = " << params.target_spacing
              << ", epsilon = " << params.epsilon << std::endl;

    auto resampled_line_map = std::map<TimeFrameIndex, std::vector<Line2D>>();
    int total_lines = 0;
    for (auto const & time_lines_pair: line_data->GetAllLinesAsRange()) {
        total_lines += static_cast<int>(time_lines_pair.lines.size());
    }
    if (total_lines == 0) {
        if (progressCallback) {
            progressCallback(100);// No data to process, so 100% complete.
        }
        result_line_data->setImageSize(line_data->getImageSize());
        return result_line_data;
    }

    int processed_lines = 0;
    if (progressCallback) {
        progressCallback(0);
    }

    for (auto const & time_lines_pair: line_data->GetAllLinesAsRange()) {
        TimeFrameIndex time = time_lines_pair.time;
        std::vector<Line2D> new_lines_at_time;
        new_lines_at_time.reserve(time_lines_pair.lines.size());

        for (auto const & single_line: time_lines_pair.lines) {
            if (single_line.empty()) {
                new_lines_at_time.push_back(Line2D());// Keep empty lines as empty
            } else {
                Line2D simplified_line;
                switch (params.algorithm) {
                    case LineSimplificationAlgorithm::FixedSpacing:
                        simplified_line = resample_line_points(single_line, params.target_spacing);
                        break;
                    case LineSimplificationAlgorithm::DouglasPeucker:
                        simplified_line = douglas_peucker_simplify(single_line, params.epsilon);
                        break;
                }
                new_lines_at_time.push_back(simplified_line);
            }
            processed_lines++;
            if (progressCallback && total_lines > 0) {
                int const current_progress = static_cast<int>((static_cast<double>(processed_lines) / total_lines) * 100.0);
                progressCallback(current_progress);
            }
        }
        if (!new_lines_at_time.empty() || line_data->getAtTime(time).empty()) {
            // Add to map if there are new lines OR if the original time had lines (even if all became empty)
            // This ensures that times that previously had data but now have all empty lines are still represented.
            // However, if a time originally had no lines, we don't add an empty entry for it.
            if (!line_data->getAtTime(time).empty() || !new_lines_at_time.empty()) {
                resampled_line_map[time] = std::move(new_lines_at_time);
            }
        }
    }

    result_line_data = std::make_shared<LineData>(resampled_line_map);
    result_line_data->setImageSize(line_data->getImageSize());// Preserve image size

    if (progressCallback) {
        progressCallback(100);// Ensure 100% is reported at the end.
    }
    std::cout << "LineResampleOperation executed successfully." << std::endl;
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
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        return false;
    }
    auto const * ptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;// Check if it's a valid, non-null LineData
}

std::unique_ptr<TransformParametersBase> LineResampleOperation::getDefaultParameters() const {
    return std::make_unique<LineResampleParameters>();
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters) {
    // Call the version with progress reporting but ignore progress here
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineResampleOperation::execute(DataTypeVariant const & dataVariant,
                                               TransformParametersBase const * transformParameters,
                                               ProgressCallback progressCallback) {
    auto const * line_data_sptr_ptr = std::get_if<std::shared_ptr<LineData>>(&dataVariant);
    if (!line_data_sptr_ptr || !(*line_data_sptr_ptr)) {
        std::cerr << "LineResampleOperation::execute called with incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);// Indicate completion even on error
        return {};// Return empty variant
    }
    LineData const * input_line_data = (*line_data_sptr_ptr).get();

    LineResampleParameters currentParams;// Default parameters

    if (transformParameters != nullptr) {
        auto const * specificParams = dynamic_cast<LineResampleParameters const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
            // std::cout << "Using parameters provided by UI." << std::endl; // Debug, consider removing
        } else {
            std::cerr << "Warning: LineResampleOperation received incompatible parameter type (dynamic_cast failed)! Using default parameters." << std::endl;
            // Fall through to use the default 'currentParams'
        }
    } else {
        // std::cout << "LineResampleOperation received null parameters. Using default parameters." << std::endl; // Debug, consider removing
        // Fall through to use the default 'currentParams'
    }

    std::shared_ptr<LineData> result_line_data = line_resample(input_line_data,
                                                               currentParams,
                                                               progressCallback);

    if (!result_line_data) {
        std::cerr << "LineResampleOperation::execute: 'line_resample' failed to produce a result." << std::endl;
        if (progressCallback) progressCallback(100);// Indicate completion even on error
        return {};// Return empty
    }

    // std::cout << "LineResampleOperation executed successfully using variant input." << std::endl; // Debug, consider removing
    if (progressCallback) progressCallback(100);// Ensure 100% is reported at the end.
    return result_line_data;
}
