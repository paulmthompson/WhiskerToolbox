/**
 * @file InferenceController.cpp
 * @brief Implementation of inference orchestration for deep learning.
 */

#include "InferenceController.hpp"

#include "DeepLearning_Widget/Core/DeepLearningBindingData.hpp"
#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/Core/ResultProcessor.hpp"
#include "DeepLearning_Widget/Core/SlotAssembler.hpp"
#include "DeepLearning_Widget/Core/WriteReservation.hpp"

#include "DataManager/DataManager.hpp"
#include "Media/Media_Data.hpp"
#include "Media/Video_Data.hpp"

#include <QThread>

#include <atomic>
#include <memory>

// ════════════════════════════════════════════════════════════════════════════
// BatchInferenceWorker — runs SlotAssembler::runBatchRangeOffline on a
// background QThread.
// ════════════════════════════════════════════════════════════════════════════

namespace {

class BatchInferenceWorker : public QThread {
    Q_OBJECT

public:
    BatchInferenceWorker(
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
            QObject * parent = nullptr)
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

    [[nodiscard]] bool success() const { return _success; }

    [[nodiscard]] std::string const & errorMessage() const { return _error_message; }

signals:
    void progressChanged(int _t1, int _t2);

protected:
    void run() override {
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

private:
    SlotAssembler * _assembler;
    DataManager * _dm;
    SlotAssembler::MediaOverrides _media_overrides;
    std::vector<SlotBindingData> _input_bindings;
    std::vector<StaticInputData> _static_inputs;
    std::vector<OutputBindingData> _output_bindings;
    int _start_frame;
    int _end_frame;
    ImageSize _source_image_size;
    int _batch_size;
    std::shared_ptr<WriteReservation> _reservation;
    std::shared_ptr<std::atomic<bool>> _cancel_flag;
    bool _success = true;
    std::string _error_message;
};

// ════════════════════════════════════════════════════════════════════════════
// MultiIntervalBatchWorker — runs SlotAssembler::runBatchRangeOffline on a
// background QThread for multiple frame intervals with unified progress.
// ════════════════════════════════════════════════════════════════════════════

class MultiIntervalBatchWorker : public QThread {
    Q_OBJECT

public:
    MultiIntervalBatchWorker(
            SlotAssembler * assembler,
            DataManager * dm,
            SlotAssembler::MediaOverrides media_overrides,
            std::vector<SlotBindingData> input_bindings,
            std::vector<StaticInputData> static_inputs,
            std::vector<OutputBindingData> output_bindings,
            std::vector<std::pair<int64_t, int64_t>> intervals,
            ImageSize source_image_size,
            int batch_size,
            std::shared_ptr<WriteReservation> reservation,
            std::shared_ptr<std::atomic<bool>> cancel_flag,
            QObject * parent = nullptr)
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

    [[nodiscard]] bool success() const { return _success; }

    [[nodiscard]] std::string const & errorMessage() const { return _error_message; }

signals:
    void progressChanged(int _t1, int _t2);

protected:
    void run() override {
        // Ensure CUDA context on this worker thread (see BatchInferenceWorker).
        //SlotAssembler::initDeviceForCurrentThread();

        // Compute total frame count across all intervals
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

private:
    SlotAssembler * _assembler;
    DataManager * _dm;
    SlotAssembler::MediaOverrides _media_overrides;
    std::vector<SlotBindingData> _input_bindings;
    std::vector<StaticInputData> _static_inputs;
    std::vector<OutputBindingData> _output_bindings;
    std::vector<std::pair<int64_t, int64_t>> _intervals;
    ImageSize _source_image_size;
    int _batch_size;
    std::shared_ptr<WriteReservation> _reservation;
    std::shared_ptr<std::atomic<bool>> _cancel_flag;
    bool _success = true;
    std::string _error_message;
};

}// namespace

// ════════════════════════════════════════════════════════════════════════════
// InferenceController::Impl
// ════════════════════════════════════════════════════════════════════════════

struct InferenceController::Impl {
    SlotAssembler * _assembler = nullptr;
    std::shared_ptr<DataManager> _dm;
    std::shared_ptr<DeepLearningState> _state;
    std::unique_ptr<ResultProcessor> _result_processor;

    QThread * _batch_worker = nullptr;

    /// Shared cancellation flag; set by cancel(), read by the active worker.
    std::shared_ptr<std::atomic<bool>> _cancel_flag =
            std::make_shared<std::atomic<bool>>(false);
};

// ════════════════════════════════════════════════════════════════════════════
// InferenceController
// ════════════════════════════════════════════════════════════════════════════

InferenceController::InferenceController(SlotAssembler * assembler,
                                         std::shared_ptr<DataManager> dm,
                                         std::shared_ptr<DeepLearningState> state,
                                         QObject * parent)
    : QObject(parent),
      _impl(std::make_unique<Impl>()) {
    _impl->_assembler = assembler;
    _impl->_dm = std::move(dm);
    _impl->_state = std::move(state);
    _impl->_result_processor = std::make_unique<ResultProcessor>(
            _impl->_dm, _impl->_state, this);
}

InferenceController::~InferenceController() {
    if (_impl->_batch_worker) {
        _impl->_cancel_flag->store(true, std::memory_order_relaxed);
        _impl->_batch_worker->wait();
        delete _impl->_batch_worker;
        _impl->_batch_worker = nullptr;
    }
}

bool InferenceController::isRunning() const {
    return _impl->_batch_worker != nullptr;
}

void InferenceController::runSingleFrame(int frame) {
    if (!_impl->_assembler->isModelReady()) {
        emit batchFinished(false, tr("Model weights not loaded."));
        return;
    }

    try {
        ImageSize source_size{256, 256};
        for (auto const & binding: _impl->_state->inputBindings()) {
            auto media = _impl->_dm->getData<MediaData>(binding.data_key);
            if (media) {
                source_size = media->getImageSize();
                break;
            }
        }

        _impl->_assembler->runSingleFrame(
                *_impl->_dm,
                _impl->_state->inputBindings(),
                _impl->_state->staticInputs(),
                _impl->_state->outputBindings(),
                frame,
                source_size);

        emit batchFinished(true, QString{});
    } catch (std::exception const & e) {
        emit batchFinished(false, QString::fromUtf8(e.what()));
    }
}

void InferenceController::runBatch(int start, int end, int batch_size) {
    if (_impl->_batch_worker) {
        cancel();
        return;
    }

    if (!_impl->_assembler->isModelReady()) {
        emit batchFinished(false, tr("Model weights not loaded."));
        return;
    }

    if (end < start) {
        emit batchFinished(false, tr("End frame must be >= start frame."));
        return;
    }

    _impl->_result_processor->clear();

    ImageSize source_size{256, 256};
    for (auto const & binding: _impl->_state->inputBindings()) {
        auto media = _impl->_dm->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    SlotAssembler::MediaOverrides media_overrides;
    for (auto const & binding: _impl->_state->inputBindings()) {
        if (binding.encoder_id != "ImageEncoder" || binding.data_key.empty())
            continue;
        auto media = _impl->_dm->getData<MediaData>(binding.data_key);
        if (!media) continue;
        if (media->getMediaType() == MediaData::MediaType::Video) {
            auto clone = std::make_shared<VideoData>();
            clone->LoadMedia(media->getFilename());
            media_overrides[binding.data_key] = std::move(clone);
        } else {
            media_overrides[binding.data_key] = media;
        }
    }

    auto reservation = std::make_shared<WriteReservation>();

    _impl->_cancel_flag->store(false, std::memory_order_relaxed);

    auto * worker = new BatchInferenceWorker(
            _impl->_assembler,
            _impl->_dm.get(),
            std::move(media_overrides),
            _impl->_state->inputBindings(),
            _impl->_state->staticInputs(),
            _impl->_state->outputBindings(),
            start,
            end,
            source_size,
            batch_size,
            reservation,
            _impl->_cancel_flag,
            this);

    connect(worker, &BatchInferenceWorker::progressChanged,
            this, &InferenceController::batchProgressChanged);

    connect(worker, &QThread::finished, this, [this, worker]() {
        _impl->_result_processor->stopMergeTimer();
        _impl->_result_processor->flushFeatureVectors();

        bool const success = worker->success();
        QString const error_msg =
                success ? QString{} : QString::fromStdString(worker->errorMessage());

        worker->deleteLater();
        _impl->_batch_worker = nullptr;

        emit batchFinished(success, error_msg);
        emit runningChanged(false);
    });

    _impl->_result_processor->setReservation(reservation);
    _impl->_result_processor->startMergeTimer();
    _impl->_batch_worker = worker;
    emit runningChanged(true);

    worker->start();
}

void InferenceController::runBatchIntervals(
        std::vector<std::pair<int64_t, int64_t>> intervals,
        int batch_size) {
    if (_impl->_batch_worker) {
        cancel();
        return;
    }

    if (!_impl->_assembler->isModelReady()) {
        emit batchFinished(false, tr("Model weights not loaded."));
        return;
    }

    if (intervals.empty()) {
        emit batchFinished(false, tr("No intervals to process."));
        return;
    }

    _impl->_result_processor->clear();

    ImageSize source_size{256, 256};
    for (auto const & binding: _impl->_state->inputBindings()) {
        auto media = _impl->_dm->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    SlotAssembler::MediaOverrides media_overrides;
    for (auto const & binding: _impl->_state->inputBindings()) {
        if (binding.encoder_id != "ImageEncoder" || binding.data_key.empty())
            continue;
        auto media = _impl->_dm->getData<MediaData>(binding.data_key);
        if (!media) continue;
        if (media->getMediaType() == MediaData::MediaType::Video) {
            auto clone = std::make_shared<VideoData>();
            clone->LoadMedia(media->getFilename());
            media_overrides[binding.data_key] = std::move(clone);
        } else {
            media_overrides[binding.data_key] = media;
        }
    }

    auto reservation = std::make_shared<WriteReservation>();

    _impl->_cancel_flag->store(false, std::memory_order_relaxed);

    auto * worker = new MultiIntervalBatchWorker(
            _impl->_assembler,
            _impl->_dm.get(),
            std::move(media_overrides),
            _impl->_state->inputBindings(),
            _impl->_state->staticInputs(),
            _impl->_state->outputBindings(),
            std::move(intervals),
            source_size,
            batch_size,
            reservation,
            _impl->_cancel_flag,
            this);

    connect(worker, &MultiIntervalBatchWorker::progressChanged,
            this, &InferenceController::batchProgressChanged);

    connect(worker, &QThread::finished, this, [this, worker]() {
        _impl->_result_processor->stopMergeTimer();
        _impl->_result_processor->flushFeatureVectors();

        bool const success = worker->success();
        QString const error_msg =
                success ? QString{} : QString::fromStdString(worker->errorMessage());

        worker->deleteLater();
        _impl->_batch_worker = nullptr;

        emit batchFinished(success, error_msg);
        emit runningChanged(false);
    });

    _impl->_result_processor->setReservation(reservation);
    _impl->_result_processor->startMergeTimer();
    _impl->_batch_worker = worker;
    emit runningChanged(true);

    worker->start();
}

void InferenceController::runRecurrentSequence(int start, int frame_count) {
    if (!_impl->_assembler->isModelReady()) {
        emit batchFinished(false, tr("Model weights not loaded."));
        return;
    }

    auto const & recurrent = _impl->_state->recurrentBindings();
    if (recurrent.empty()) {
        emit batchFinished(false,
                           tr("Configure at least one recurrent feedback binding."));
        return;
    }

    ImageSize source_size{256, 256};
    for (auto const & binding: _impl->_state->inputBindings()) {
        auto media = _impl->_dm->getData<MediaData>(binding.data_key);
        if (media) {
            source_size = media->getImageSize();
            break;
        }
    }

    try {
        _impl->_assembler->clearRecurrentCache();

        _impl->_assembler->runRecurrentSequence(
                *_impl->_dm,
                _impl->_state->inputBindings(),
                _impl->_state->staticInputs(),
                _impl->_state->outputBindings(),
                recurrent,
                start,
                frame_count,
                source_size,
                [this](int current, int total) {
                    emit recurrentProgressChanged(current, total);
                });

        emit batchFinished(true, QString{});
    } catch (std::exception const & e) {
        emit batchFinished(false, QString::fromUtf8(e.what()));
    }
}

void InferenceController::cancel() {
    if (!_impl->_batch_worker) return;
    _impl->_cancel_flag->store(true, std::memory_order_relaxed);
}

#include "InferenceController.moc"
