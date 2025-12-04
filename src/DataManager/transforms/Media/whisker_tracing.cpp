#include "whisker_tracing.hpp"

#include "Masks/Mask_Data.hpp"
#include "whiskertracker.hpp"
#include "producer_consumer_pipeline.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <omp.h>

#include <algorithm>
#include <chrono>
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

        auto t0 = std::chrono::high_resolution_clock::now();

        std::vector<std::vector<uint8_t>> masks;
        masks.reserve(images.size());

        for (size_t i = 0; i < images.size(); ++i) {
            int const time_idx = (i < time_indices.size()) ? time_indices[i] : 0;
            auto binary_mask = convert_mask_to_binary(mask_data, time_idx, image_size);
            masks.push_back(std::move(binary_mask));
        }

        //auto t1 = std::chrono::high_resolution_clock::now();

        //std::cout << "Mask Generation: " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "ms" << std::endl;

        auto whiskers_batch = whisker_tracker.trace_multiple_images_with_masks(images, masks, image_size.height, image_size.width);

        //auto t2 = std::chrono::high_resolution_clock::now();

        //std::cout << "Mask Tracing: " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "ms" << std::endl;


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
    return canApplyToType<MediaData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> WhiskerTracingOperation::getDefaultParameters() const {
    return std::make_unique<WhiskerTracingParameters>();
}

DataTypeVariant WhiskerTracingOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

/**
 * @brief A simple data structure to hold a frame's image data and its timestamp.
 *
 * This replaces the original FrameData struct, removing the is_end_marker
 * which is no longer needed with the new BlockingQueue design.
 */
struct MediaFrame {
    std::vector<uint8_t> image_data;
    int time_index;
};


DataTypeVariant WhiskerTracingOperation::execute(DataTypeVariant const& dataVariant,
                                                 TransformParametersBase const* transformParameters,
                                                 ProgressCallback progressCallback) {
    auto const* ptr_ptr = std::get_if<std::shared_ptr<MediaData>>(&dataVariant);
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "WhiskerTracingOperation::execute: Incompatible variant type or null data." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    auto media_data = *ptr_ptr;
    auto const* params = dynamic_cast<WhiskerTracingParameters const*>(transformParameters);

    if (!params) {
        std::cerr << "WhiskerTracingOperation::execute: Invalid parameters." << std::endl;
        if (progressCallback) progressCallback(100);
        return {};
    }

    std::shared_ptr<whisker::WhiskerTracker> tracker = params->tracker;
    if (!tracker) {
        tracker = std::make_shared<whisker::WhiskerTracker>();
        std::cout << "Whisker Tracker Initialized" << std::endl;
    }
    tracker->setWhiskerLengthThreshold(params->whisker_length_threshold);
    tracker->setWhiskerPadRadius(1000.0f);

    if (progressCallback) progressCallback(0);

    auto traced_whiskers = std::make_shared<LineData>();
    traced_whiskers->setImageSize(media_data->getImageSize());

    auto total_frame_count = media_data->getTotalFrameCount();
    if (total_frame_count <= 0) {
        if (progressCallback) progressCallback(100);
        return {};
    }

    if (params->use_parallel_processing && params->batch_size > 1) {
        // --- Producer-Consumer Parallel Processing ---

        // BUG FIX: The original code was not thread-safe if MediaData is not.
        // This mutex protects access to media_data from the producer thread.
        std::mutex media_data_mutex;

        // The consumer will update the final LineData object. This mutex protects it.
        std::mutex results_mutex;

        // Define the producer logic as a lambda function.
        auto producer = [&](size_t frame_idx) -> std::optional<MediaFrame> {
            std::vector<uint8_t> image_data;
            try {
                // Lock the mutex before accessing media_data
                std::lock_guard<std::mutex> lock(media_data_mutex);
                if (params->use_processed_data) {
                    image_data = media_data->getProcessedData8(frame_idx);
                } else {
                    image_data = media_data->getRawData8(frame_idx);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error producing frame " << frame_idx << ": " << e.what() << std::endl;
                return std::nullopt; // Signal failure for this item
            }

            if (image_data.empty()) {
                return std::nullopt; // Can happen if a frame is invalid
            }
            return MediaFrame{std::move(image_data), static_cast<int>(frame_idx)};
        };

        // Define the consumer logic as a lambda function.
        auto consumer = [&](std::vector<MediaFrame> batch) {
            std::vector<std::vector<uint8_t>> batch_images;
            std::vector<int> batch_times;
            batch_images.reserve(batch.size());
            batch_times.reserve(batch.size());

            for (auto& frame : batch) {
                batch_images.push_back(std::move(frame.image_data));
                batch_times.push_back(frame.time_index);
            }

            auto batch_results = trace_multiple_images(*tracker,
                                                       batch_images,
                                                       media_data->getImageSize(),
                                                       params->clip_length,
                                                       params->use_mask_data ? params->mask_data.get() : nullptr,
                                                       batch_times);
            
            // Lock the mutex to safely update the shared results container
            std::lock_guard<std::mutex> lock(results_mutex);
            for (size_t j = 0; j < batch_results.size(); ++j) {
                for (auto const& line : batch_results[j]) {
                    traced_whiskers->addAtTime(TimeFrameIndex(batch_times[j]), line, NotifyObservers::No);
                }
            }
        };

        // Set OpenMP threads. For an application-wide effect, this is okay.
        // For library code, using the 'num_threads' clause on the pragma is safer.
        int max_threads = omp_get_max_threads();
        int omp_threads = std::max(1, max_threads - 1); // Reserve 1 core for producer
        omp_set_num_threads(omp_threads);
        std::cout << "Using " << omp_threads << " OpenMP threads for processing." << std::endl;

        // Execute the pipeline
        run_pipeline<MediaFrame>(
            params->queue_size,
            total_frame_count,
            producer,
            consumer,
            params->batch_size,
            progressCallback);

    } else {
        // Process frames one by one (original sequential approach)
        for (size_t time = 0; time < total_frame_count; ++time) {
            std::vector<uint8_t> image_data;

            if (params->use_processed_data) {
                image_data = media_data->getProcessedData8(static_cast<int>(time));
            } else {
                image_data = media_data->getRawData8(static_cast<int>(time));
            }

            if (!image_data.empty()) {
                auto whisker_lines = trace_single_image(*tracker, image_data, media_data->getImageSize(),
                                                        params->clip_length,
                                                        params->use_mask_data ? params->mask_data.get() : nullptr,
                                                        static_cast<int>(time));

                for (auto const & line: whisker_lines) {
                    traced_whiskers->addAtTime(TimeFrameIndex(static_cast<int64_t>(time)), line, NotifyObservers::No);
                }
            }

            if (progressCallback) {
                int const current_progress = static_cast<int>(std::round(static_cast<double>(time) / static_cast<double>(total_frame_count) * PROGRESS_SCALE));
                progressCallback(current_progress);
            }
        }
    }

    if (progressCallback) progressCallback(PROGRESS_COMPLETE);

    std::cout << "WhiskerTracingOperation executed successfully. Traced "
              << traced_whiskers->getTotalEntryCount() << " whiskers across "
              << total_frame_count << " frames." << std::endl;

    return traced_whiskers;
}
