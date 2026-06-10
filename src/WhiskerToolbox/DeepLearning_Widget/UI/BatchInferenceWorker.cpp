/**
 * @file BatchInferenceWorker.cpp
 * @brief Implementation of single-range batch inference worker thread.
 */

#include "BatchInferenceWorker.hpp"

#include "DataManager/DataManager.hpp"

#include <utility>

BatchInferenceWorker::BatchInferenceWorker(
        SlotAssembler * assembler,
        DataManager * dm,
        SlotAssembler::MediaOverrides media_overrides,
        std::vector<SlotBindingData> input_bindings,
        std::vector<StaticInputData> static_inputs,
        std::vector<OutputBindingData> output_bindings,
        int start_frame,
        int end_frame,
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
      _start_frame(start_frame),
      _end_frame(end_frame),
      _source_image_size(source_image_size),
      _batch_size(batch_size),
      _reservation(std::move(reservation)),
      _cancel_flag(std::move(cancel_flag)) {}

bool BatchInferenceWorker::success() const {
    return _success;
}

std::string const & BatchInferenceWorker::errorMessage() const {
    return _error_message;
}

void BatchInferenceWorker::run() {
    // Ensure the CUDA runtime context is initialized on this worker
    // thread. On Windows, CUDA per-thread state is not inherited
    // from the main thread.
    //SlotAssembler::initDeviceForCurrentThread();

    auto result = _assembler->runBatchRangeOffline(
            *_dm,
            _media_overrides,
            _input_bindings,
            _static_inputs,
            _output_bindings,
            _start_frame,
            _end_frame,
            _source_image_size,
            *_cancel_flag,
            _batch_size,
            [this](int current, int total) {
                emit progressChanged(current, total);
            },
            [this](std::vector<FrameResult> frame_results) {
                _reservation->push(std::move(frame_results));
            });
    _success = result.success;
    _error_message = std::move(result.error_message);
}
