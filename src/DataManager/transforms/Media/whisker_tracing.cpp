#include "whisker_tracing.hpp"

#include "whiskertracker.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

// Convert whisker::Line2D to Line2D
Line2D WhiskerTracingOperation::convert_to_Line2D(whisker::Line2D const & whisker_line) {
    Line2D line;

    for (auto const & point: whisker_line) {
        line.push_back(Point2D<float>{point.x, point.y});
    }

    return line;
}

// Clip whisker line by removing points from the end
void WhiskerTracingOperation::clip_whisker(Line2D & line, int clip_length) {
    if (line.size() <= static_cast<std::size_t>(clip_length)) {
        return;
    }

    line.erase(line.end() - clip_length, line.end());
}

// Trace whiskers in a single image
std::vector<Line2D> WhiskerTracingOperation::trace_single_image(
        whisker::WhiskerTracker & whisker_tracker,
        std::vector<uint8_t> const & image_data,
        ImageSize const & image_size,
        int clip_length) {
    auto whiskers = whisker_tracker.trace(image_data, image_size.height, image_size.width);

    std::vector<Line2D> whisker_lines;
    whisker_lines.reserve(whiskers.size());

    for (auto const & whisker: whiskers) {
        Line2D line = convert_to_Line2D(whisker);
        clip_whisker(line, clip_length);
        whisker_lines.push_back(std::move(line));
    }

    return whisker_lines;
}

// Trace whiskers in multiple images in parallel
std::vector<std::vector<Line2D>> WhiskerTracingOperation::trace_multiple_images(
        whisker::WhiskerTracker & whisker_tracker,
        std::vector<std::vector<uint8_t>> const & images,
        ImageSize const & image_size,
        int clip_length) {


    // Use the parallel tracing method if available
    auto whiskers_batch = whisker_tracker.trace_multiple_images(images, image_size.height, image_size.width);

    std::vector<std::vector<Line2D>> result;
    result.reserve(whiskers_batch.size());

    for (auto const & whiskers: whiskers_batch) {
        std::vector<Line2D> whisker_lines;
        whisker_lines.reserve(whiskers.size());

        for (auto const & whisker: whiskers) {
            Line2D line = convert_to_Line2D(whisker);
            clip_whisker(line, clip_length);
            whisker_lines.push_back(std::move(line));
        }

        result.push_back(std::move(whisker_lines));
    }

    return result;
}

std::string WhiskerTracingOperation::getName() const {
    return "Whisker Tracing";
}

std::type_index WhiskerTracingOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<MediaData>);
}

bool WhiskerTracingOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<MediaData>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<MediaData>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> WhiskerTracingOperation::getDefaultParameters() const {
    return std::make_unique<WhiskerTracingParameters>();
}

DataTypeVariant WhiskerTracingOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant WhiskerTracingOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters,
                                                 ProgressCallback progressCallback) {
    auto const * ptr_ptr = std::get_if<std::shared_ptr<MediaData>>(&dataVariant);
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "WhiskerTracingOperation::execute: Incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto media_data = *ptr_ptr;

    auto const * typed_params =
            transformParameters ? dynamic_cast<WhiskerTracingParameters const *>(transformParameters) : nullptr;

    if (!typed_params) {
        std::cerr << "WhiskerTracingOperation::execute: Invalid parameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto whisker_tracker = std::make_unique<whisker::WhiskerTracker>();
    std::cout << "Whisker Tracker Initialized" << std::endl;

    whisker_tracker->setWhiskerLengthThreshold(typed_params->whisker_length_threshold);

    if (progressCallback) progressCallback(0);

    // Create new LineData for the traced whiskers
    auto traced_whiskers = std::make_shared<LineData>();
    traced_whiskers->setImageSize(media_data->getImageSize());

    // Get times with data
    auto total_frame_count = media_data->getTotalFrameCount();
    if (total_frame_count <= 0) {
        std::cerr << "WhiskerTracingOperation::execute: No data available in media." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto total_time_points = static_cast<size_t>(total_frame_count);
    size_t processed_time_points = 0;

    // Process frames in batches for parallel processing
    if (typed_params->use_parallel_processing && typed_params->batch_size > 1) {
        for (size_t i = 0; i < total_time_points; i += static_cast<size_t>(typed_params->batch_size)) {
            std::vector<std::vector<uint8_t>> batch_images;
            std::vector<int> batch_times;

            // Collect images for this batch
            for (size_t j = 0; j < static_cast<size_t>(typed_params->batch_size) && (i + j) < total_time_points; ++j) {
                auto time = i + j;
                std::vector<uint8_t> image_data;

                if (typed_params->use_processed_data) {
                    // whisker tracking expects 8 bit
                    image_data = media_data->getProcessedData8(static_cast<int>(time));
                } else {
                    image_data = media_data->getRawData8(static_cast<int>(time));
                }

                if (!image_data.empty()) {
                    batch_images.push_back(std::move(image_data));
                    batch_times.push_back(static_cast<int>(time));
                }
            }

            if (!batch_images.empty()) {
                // Trace whiskers in parallel for this batch
                auto batch_results = trace_multiple_images(*whisker_tracker, batch_images, media_data->getImageSize(), typed_params->clip_length);

                // Add results to LineData
                for (size_t j = 0; j < batch_results.size(); ++j) {
                    for (auto const & line: batch_results[j]) {
                        traced_whiskers->addAtTime(TimeFrameIndex(batch_times[j]), line, false);
                    }
                }

                processed_time_points += batch_images.size();
                std::cout << "Processed " << processed_time_points << " time points" << std::endl;
            }

            if (progressCallback) {
                int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
                progressCallback(current_progress);
            }
        }
    } else {
        // Process frames one by one
        for (size_t time = 0; time < total_time_points; ++time) {
            std::vector<uint8_t> image_data;

            if (typed_params->use_processed_data) {
                image_data = media_data->getProcessedData8(static_cast<int>(time));
            } else {
                image_data = media_data->getRawData8(static_cast<int>(time));
            }

            if (!image_data.empty()) {
                auto whisker_lines = trace_single_image(*whisker_tracker, image_data, media_data->getImageSize(), typed_params->clip_length);

                for (auto const & line: whisker_lines) {
                    traced_whiskers->addAtTime(TimeFrameIndex(static_cast<int64_t>(time)), line, false);
                }
            }

            processed_time_points++;
            if (progressCallback) {
                int current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * 100.0));
                progressCallback(current_progress);
            }
        }
    }

    if (progressCallback) progressCallback(100);

    std::cout << "WhiskerTracingOperation executed successfully. Traced "
              << traced_whiskers->GetAllLinesAsRange().size() << " whiskers across "
              << total_frame_count << " time points." << std::endl;

    return traced_whiskers;
}
