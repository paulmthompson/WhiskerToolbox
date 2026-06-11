/**
 * @file MultiIntervalBatchWorker.cpp
 * @brief Implementation of multi-interval batch inference worker thread.
 */

#include "MultiIntervalBatchWorker.hpp"

#include "DataManager/DataManager.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"

#include <utility>

MultiIntervalBatchWorker::MultiIntervalBatchWorker(
        SlotAssembler * assembler,
        DataManager * dm,
        MediaOverrides media_overrides,
        std::vector<SlotBindingData> input_bindings,
        std::vector<StaticInputData> static_inputs,
        std::vector<OutputBindingData> output_bindings,
        std::vector<std::pair<int64_t, int64_t>> intervals,
        ImageSize source_image_size,
        int batch_size,
        std::shared_ptr<WriteReservation> reservation,
        std::shared_ptr<std::atomic<bool>> cancel_flag,
        QObject * parent)
    : QThread(parent),
      _assembler(assembler),
      _dm(dm),
      _media_overrides(std::move(media_overrides)),
      _input_bindings(std::move(input_bindings)),
      _static_inputs(std::move(static_inputs)),
      _output_bindings(std::move(output_bindings)),
      _intervals(std::move(intervals)),
      _source_image_size(source_image_size),
      _batch_size(batch_size),
      _reservation(std::move(reservation)),
      _cancel_flag(std::move(cancel_flag)) {}

bool MultiIntervalBatchWorker::success() const {
    return _success;
}

std::string const & MultiIntervalBatchWorker::errorMessage() const {
    return _error_message;
}

/**
 * @brief Executes batch inference over all intervals on the worker thread.
 */
void MultiIntervalBatchWorker::run() {
    // Ensure CUDA context on this worker thread (see BatchInferenceWorker).
    //SlotAssembler::initDeviceForCurrentThread();

    int total_frames = 0;
    for (auto const & [start, end]: _intervals) {
        total_frames += static_cast<int>(end - start + 1);
    }

    int frames_completed = 0;

    for (auto const & [start, end]: _intervals) {
        if (_cancel_flag->load(std::memory_order_relaxed)) break;

        int const interval_frames = static_cast<int>(end - start + 1);
        int const offset = frames_completed;

        auto result = _assembler->runBatchRangeOffline(
                *_dm,
                _media_overrides,
                _input_bindings,
                _static_inputs,
                _output_bindings,
                static_cast<int>(start),
                static_cast<int>(end),
                _source_image_size,
                *_cancel_flag,
                _batch_size,
                [this, offset, total_frames](int current, int /*interval_total*/) {
                    emit progressChanged(offset + current, total_frames);
                },
                [this](std::vector<FrameResult> frame_results) {
                    _reservation->push(std::move(frame_results));
                });

        if (!result.success) {
            _success = false;
            _error_message = std::move(result.error_message);
            return;
        }

        frames_completed += interval_frames;
    }

    emit progressChanged(total_frames, total_frames);
}
