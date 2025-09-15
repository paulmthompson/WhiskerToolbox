#ifndef WHISKER_TRACING_HPP
#define WHISKER_TRACING_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Lines/Line_Data.hpp"
#include "Media/Media_Data.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// Forward declaration
namespace whisker {
class WhiskerTracker;
class Line2D;
}// namespace whisker

class MaskData;

/**
 * @brief Converts mask data to a binary mask image sized to the media frame.
 *
 * @pre mask_data may be nullptr; in that case an all-zero mask is returned.
 * @post Returned vector has size image_size.width * image_size.height and uses 255 for true pixels, 0 otherwise.
 */
std::vector<uint8_t> convert_mask_to_binary(MaskData const * mask_data,
                                            int time_index,
                                            ImageSize const & image_size);

/**
 * @brief Thread-safe frame data structure for producer-consumer pattern
 */
struct FrameData {
    std::vector<uint8_t> image_data;
    int time_index;
    bool is_end_marker = false; // Special marker to signal end of data
    
    FrameData() = default;
    FrameData(std::vector<uint8_t> img, int time_idx) 
        : image_data(std::move(img)), time_index(time_idx) {}
};

/**
 * @brief Thread-safe queue for producer-consumer pattern
 */
class FrameQueue {
public:
    FrameQueue(size_t max_size = 10) : max_size_(max_size) {}
    
    void push(FrameData frame) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < max_size_; });
        queue_.push(std::move(frame));
        not_empty_.notify_one();
    }
    
    bool pop(FrameData& frame, std::chrono::milliseconds timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (not_empty_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            frame = std::move(queue_.front());
            queue_.pop();
            not_full_.notify_one();
            return true;
        }
        return false;
    }
    
    void push_end_marker() {
        FrameData end_frame;
        end_frame.is_end_marker = true;
        push(std::move(end_frame));
    }
    
    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
private:
    std::queue<FrameData> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_;
};

/**
 * @brief Parameters for whisker tracing operation
 */
struct WhiskerTracingParameters : public TransformParametersBase {
    bool use_processed_data = true;        // Whether to use processed or raw data
    int clip_length = 0;                   // Number of points to clip from whisker tips
    float whisker_length_threshold = 50.0f;// Minimum whisker length threshold
    int batch_size = 100;                    // Number of frames to process in parallel
    bool use_parallel_processing = true;   // Whether to use OpenMP parallel processing
    bool use_mask_data = false;            // Whether to use mask data for seed selection
    std::shared_ptr<MaskData> mask_data;   // Optional mask data for seed selection
    /** Optional: Pre-initialized tracker to reuse (useful for tests to avoid long init). */
    std::shared_ptr<whisker::WhiskerTracker> tracker;
    /** Producer-consumer queue size (number of frames to buffer) */
    size_t queue_size = 20;
    /** Number of threads to reserve for producer (OpenMP gets max_threads - producer_threads) */
    int producer_threads = 2;
};

/**
 * @brief Whisker tracing operation that detects whiskers in media data
 */
class WhiskerTracingOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    /**
     * @brief Gets the target input type index for this operation.
     * @return The type index for MediaData.
     */
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null MediaData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the whisker tracing operation.
     * @return A unique_ptr to the default parameters.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the whisker tracing calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the MediaData object.
     * @param transformParameters Parameters for the calculation, including the media data pointer.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    /**
     * @brief Executes the whisker tracing calculation with progress callback.
     * @param dataVariant The variant holding a non-null shared_ptr to the MediaData object.
     * @param transformParameters Parameters for the calculation, including the media data pointer.
     * @param progressCallback Progress callback function.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;

private:
    /**
     * @brief Converts whisker::Line2D to Line2D
     * @param whisker_line The whisker tracker line
     * @return The converted line
     */
    static Line2D convert_to_Line2D(whisker::Line2D const & whisker_line);

    /**
     * @brief Clips a whisker line by removing the specified number of points from the end
     * @param line The line to clip
     * @param clip_length The number of points to remove
     */
    static void clip_whisker(Line2D & line, int clip_length);

    /**
     * @brief Traces whiskers in a single image
     * @param whisker_tracker The whisker tracker
     * @param image_data The image data
     * @param image_size The image dimensions
     * @param clip_length The number of points to clip from whisker tips
     * @param mask_data Optional mask data for seed selection
     * @param time_index Time index for mask data
     * @return Vector of traced whisker lines
     */
    std::vector<Line2D> trace_single_image(whisker::WhiskerTracker & whisker_tracker,
                                           std::vector<uint8_t> const & image_data,
                                           ImageSize const & image_size,
                                           int clip_length,
                                           MaskData const * mask_data = nullptr,
                                           int time_index = 0);

    /**
     * @brief Traces whiskers in multiple images in parallel
     * @param whisker_tracker The whisker tracker
     * @param images Vector of image data
     * @param image_size The image dimensions
     * @param clip_length The number of points to clip from whisker tips
     * @param mask_data Optional mask data for seed selection
     * @param time_indices Vector of time indices for mask data
     * @return Vector of vectors of traced whisker lines (one vector per image)
     */
    std::vector<std::vector<Line2D>> trace_multiple_images(
            whisker::WhiskerTracker & whisker_tracker,
            std::vector<std::vector<uint8_t>> const & images,
            ImageSize const & image_size,
            int clip_length,
            MaskData const * mask_data = nullptr,
            std::vector<int> const & time_indices = {});

    /**
     * @brief Producer thread function that loads frames from media_data
     * @param media_data The media data source
     * @param frame_queue The queue to push frames to
     * @param params The tracing parameters
     * @param total_frames Total number of frames to process
     * @param progress_atomic Atomic counter for progress tracking
     */
    void producer_thread(std::shared_ptr<MediaData> media_data,
                        FrameQueue& frame_queue,
                        WhiskerTracingParameters const* params,
                        int total_frames,
                        std::atomic<int>& progress_atomic);

    /**
     * @brief Consumer function that processes frames from the queue
     * @param frame_queue The queue to pop frames from
     * @param tracker The whisker tracker
     * @param image_size The image dimensions
     * @param params The tracing parameters
     * @param traced_whiskers The output LineData
     * @param progress_atomic Atomic counter for progress tracking
     * @param total_frames Total number of frames to process
     * @param progressCallback Progress callback function
     */
    void consumer_processing(FrameQueue& frame_queue,
                            whisker::WhiskerTracker& tracker,
                            ImageSize const& image_size,
                            WhiskerTracingParameters const* params,
                            std::shared_ptr<LineData> traced_whiskers,
                            std::atomic<int>& progress_atomic,
                            int total_frames,
                            ProgressCallback progressCallback);

};

#endif// WHISKER_TRACING_HPP
