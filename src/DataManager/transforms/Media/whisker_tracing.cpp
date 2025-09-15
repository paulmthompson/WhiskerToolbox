#include "whisker_tracing.hpp"

#include "Masks/Mask_Data.hpp"
#include "whiskertracker.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace {
constexpr uint8_t MASK_TRUE_VALUE = 255;
constexpr int PROGRESS_COMPLETE = 100;
constexpr double PROGRESS_SCALE = 100.0;
}// namespace

// Convert whisker::Line2D to Line2D
Line2D WhiskerTracingOperation::convert_to_Line2D(whisker::Line2D const & whisker_line) {
    Line2D line;

    for (auto const & point: whisker_line) {
        line.push_back(Point2D<float>{point.x, point.y});
    }

    return line;
}

// Convert mask data to binary mask format for whisker tracker
std::vector<uint8_t> convert_mask_to_binary(MaskData const * mask_data,
                                                                     int time_index,
                                                                     ImageSize const & image_size) {
    std::vector<uint8_t> binary_mask(static_cast<size_t>(image_size.width * image_size.height), 0);

    if (!mask_data) {
        return binary_mask;// Return empty mask if no mask data
    }

    // Get mask at the specified time
    auto const & masks_at_time = mask_data->getAtTime(TimeFrameIndex(time_index));
    if (masks_at_time.empty()) {
        return binary_mask;// Return empty mask if no mask at this time
    }

    // Source mask image size (may differ from target media image size)
    auto const src_size = mask_data->getImageSize();

    // Fast path: identical sizes, just map points directly
    if (src_size.width == image_size.width && src_size.height == image_size.height) {
        for (auto const & mask: masks_at_time) {
            for (auto const & point: mask) {
                if (point.x < image_size.width && point.y < image_size.height) {
                    auto const index = static_cast<size_t>(point.y) * static_cast<size_t>(image_size.width)
                                     + static_cast<size_t>(point.x);
                    if (index < binary_mask.size()) {
                        binary_mask[index] = MASK_TRUE_VALUE;// Set to 255 for true pixels
                    }
                }
            }
        }
        return binary_mask;
    }

    // Build a source binary mask for nearest-neighbor scaling when sizes differ
    std::vector<uint8_t> src_binary(static_cast<size_t>(src_size.width * src_size.height), 0);
    for (auto const & mask: masks_at_time) {
        for (auto const & point: mask) {
            if (point.x < src_size.width && point.y < src_size.height) {
                auto const src_index = static_cast<size_t>(point.y) * static_cast<size_t>(src_size.width)
                                     + static_cast<size_t>(point.x);
                if (src_index < src_binary.size()) {
                    src_binary[src_index] = MASK_TRUE_VALUE;
                }
            }
        }
    }

    // Nearest-neighbor scale from src_binary (src_size) to binary_mask (image_size)
    auto const src_w = std::max(1, src_size.width);
    auto const src_h = std::max(1, src_size.height);
    auto const dst_w = std::max(1, image_size.width);
    auto const dst_h = std::max(1, image_size.height);

    // Precompute ratios; use (N-1) mapping to preserve endpoints
    auto const rx = (dst_w > 1 && src_w > 1)
                            ? (static_cast<double>(src_w - 1) / static_cast<double>(dst_w - 1))
                            : 0.0;
    auto const ry = (dst_h > 1 && src_h > 1)
                            ? (static_cast<double>(src_h - 1) / static_cast<double>(dst_h - 1))
                            : 0.0;

    for (int y = 0; y < dst_h; ++y) {
        int const ys = (dst_h > 1 && src_h > 1)
                               ? static_cast<int>(std::round(static_cast<double>(y) * ry))
                               : 0;
        for (int x = 0; x < dst_w; ++x) {
            int const xs = (dst_w > 1 && src_w > 1)
                                   ? static_cast<int>(std::round(static_cast<double>(x) * rx))
                                   : 0;

            auto const src_index = static_cast<size_t>(ys) * static_cast<size_t>(src_w)
                                 + static_cast<size_t>(xs);
            auto const dst_index = static_cast<size_t>(y) * static_cast<size_t>(dst_w)
                                 + static_cast<size_t>(x);

            if (src_index < src_binary.size() && dst_index < binary_mask.size()) {
                binary_mask[dst_index] = src_binary[src_index] ? MASK_TRUE_VALUE : 0;
            }
        }
    }

    return binary_mask;
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
        int clip_length,
        MaskData const * mask_data,
        int time_index) {

    std::vector<Line2D> whisker_lines;

    if (mask_data) {
        // Use mask-based tracing
        auto binary_mask = convert_mask_to_binary(mask_data, time_index, image_size);
        auto whiskers = whisker_tracker.trace_with_mask(image_data, binary_mask, image_size.height, image_size.width);

        whisker_lines.reserve(whiskers.size());
        for (auto const & whisker: whiskers) {
            Line2D line = convert_to_Line2D(whisker);
            clip_whisker(line, clip_length);
            whisker_lines.push_back(std::move(line));
        }
    } else {
        // Use standard tracing
        auto whiskers = whisker_tracker.trace(image_data, image_size.height, image_size.width);

        whisker_lines.reserve(whiskers.size());
        for (auto const & whisker: whiskers) {
            Line2D line = convert_to_Line2D(whisker);
            clip_whisker(line, clip_length);
            whisker_lines.push_back(std::move(line));
        }
    }

    return whisker_lines;
}

// Trace whiskers in multiple images in parallel
std::vector<std::vector<Line2D>> WhiskerTracingOperation::trace_multiple_images(
        whisker::WhiskerTracker & whisker_tracker,
        std::vector<std::vector<uint8_t>> const & images,
        ImageSize const & image_size,
        int clip_length,
        MaskData const * mask_data,
        std::vector<int> const & time_indices) {

    std::vector<std::vector<Line2D>> result;
    result.reserve(images.size());

    if (mask_data && !time_indices.empty()) {
        // Use mask-based parallel tracing
        std::vector<std::vector<uint8_t>> masks;
        masks.reserve(images.size());

        for (size_t i = 0; i < images.size(); ++i) {
            int const time_idx = (i < time_indices.size()) ? time_indices[i] : 0;
            auto binary_mask = convert_mask_to_binary(mask_data, time_idx, image_size);
            masks.push_back(std::move(binary_mask));
        }

        auto whiskers_batch = whisker_tracker.trace_multiple_images_with_masks(images, masks, image_size.height, image_size.width);

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
    } else {
        // Use standard parallel tracing
        auto whiskers_batch = whisker_tracker.trace_multiple_images(images, image_size.height, image_size.width);

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
        if (progressCallback) progressCallback(PROGRESS_COMPLETE);
        return {};
    }

    auto media_data = *ptr_ptr;

    auto const * typed_params =
            transformParameters ? dynamic_cast<WhiskerTracingParameters const *>(transformParameters) : nullptr;

    if (!typed_params) {
        std::cerr << "WhiskerTracingOperation::execute: Invalid parameters." << std::endl;
        if (progressCallback) progressCallback(PROGRESS_COMPLETE);
        return {};
    }

    // Allow caller (tests) to pass an already-initialized tracker to avoid heavy setup
    std::shared_ptr<whisker::WhiskerTracker> tracker_ptr = typed_params->tracker;
    if (!tracker_ptr) {
        tracker_ptr = std::make_shared<whisker::WhiskerTracker>();
        std::cout << "Whisker Tracker Initialized" << std::endl;
    }
    tracker_ptr->setWhiskerLengthThreshold(typed_params->whisker_length_threshold);
    // Disable whisker pad exclusion by using a large radius by default
    tracker_ptr->setWhiskerPadRadius(1000.0f);

    if (progressCallback) progressCallback(0);

    // Create new LineData for the traced whiskers
    auto traced_whiskers = std::make_shared<LineData>();
    traced_whiskers->setImageSize(media_data->getImageSize());

    // Get times with data
    auto total_frame_count = media_data->getTotalFrameCount();
    if (total_frame_count <= 0) {
        std::cerr << "WhiskerTracingOperation::execute: No data available in media." << std::endl;
        if (progressCallback) progressCallback(PROGRESS_COMPLETE);
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
                auto batch_results = trace_multiple_images(*tracker_ptr, batch_images, media_data->getImageSize(),
                                                           typed_params->clip_length,
                                                           typed_params->use_mask_data ? typed_params->mask_data.get() : nullptr,
                                                           batch_times);

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
                int const current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * PROGRESS_SCALE));
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
                auto whisker_lines = trace_single_image(*tracker_ptr, image_data, media_data->getImageSize(),
                                                        typed_params->clip_length,
                                                        typed_params->use_mask_data ? typed_params->mask_data.get() : nullptr,
                                                        static_cast<int>(time));

                for (auto const & line: whisker_lines) {
                    traced_whiskers->addAtTime(TimeFrameIndex(static_cast<int64_t>(time)), line, false);
                }
            }

            processed_time_points++;
            if (progressCallback) {
                int const current_progress = static_cast<int>(std::round(static_cast<double>(processed_time_points) / static_cast<double>(total_time_points) * PROGRESS_SCALE));
                progressCallback(current_progress);
            }
        }
    }

    if (progressCallback) progressCallback(PROGRESS_COMPLETE);

    std::cout << "WhiskerTracingOperation executed successfully. Traced "
              << traced_whiskers->GetAllLinesAsRange().size() << " whiskers across "
              << total_frame_count << " time points." << std::endl;

    return traced_whiskers;
}
