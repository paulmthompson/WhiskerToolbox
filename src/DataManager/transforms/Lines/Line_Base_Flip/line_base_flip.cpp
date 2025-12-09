#include "line_base_flip.hpp"

#include "Lines/Line_Data.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <cmath>

bool LineBaseFlipTransform::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

DataTypeVariant LineBaseFlipTransform::execute(DataTypeVariant const & dataVariant,
                                              TransformParametersBase const * transformParameters) {
    // Default progress callback that does nothing
    auto defaultCallback = [](int) {};
    return execute(dataVariant, transformParameters, defaultCallback);
}

DataTypeVariant LineBaseFlipTransform::execute(DataTypeVariant const & dataVariant,
                                              TransformParametersBase const * transformParameters,
                                              ProgressCallback progressCallback) {
    if (!canApply(dataVariant)) {
        return dataVariant;
    }

    auto const & input_line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    if (!input_line_data) {
        return dataVariant;
    }

    // Cast parameters
    auto const * params = dynamic_cast<LineBaseFlipParameters const *>(transformParameters);
    if (!params) {
        return dataVariant;
    }

    // Create new LineData for output
    auto output_line_data = std::make_shared<LineData>();
    
    // Copy image size from input
    output_line_data->setImageSize(input_line_data->getImageSize());

    // Get all times with data for progress tracking
    auto times_with_data = input_line_data->getTimesWithData();
    if (times_with_data.empty()) {
        progressCallback(100);
        return output_line_data;
    }

    progressCallback(0);

    size_t processed_times = 0;
    for (auto time : times_with_data) {
        // Get lines at this time
        auto const & lines = input_line_data->getAtTime(time);

        // Process each line in the frame
        for (auto const & line : lines) {
            if (line.empty()) {
                continue;
            }
            
            Line2D processed_line;
            if (is_distal_end_closer(line, params->reference_point)) {
                processed_line = reverse_line(line);
            } else {
                processed_line = line;
            }

            // Add processed line to output
            if (!processed_line.empty()) {
                output_line_data->addAtTime(time, processed_line, NotifyObservers::No);
            }
        }

        // Update progress
        processed_times++;
        int progress = static_cast<int>(
            std::round(static_cast<double>(processed_times) / static_cast<double>(times_with_data.size()) * 100.0)
        );
        progressCallback(progress);
    }

    progressCallback(100);
    return output_line_data;
}
