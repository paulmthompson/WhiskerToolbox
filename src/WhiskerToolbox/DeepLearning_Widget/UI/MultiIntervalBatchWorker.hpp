#ifndef MULTI_INTERVAL_BATCH_WORKER_HPP
#define MULTI_INTERVAL_BATCH_WORKER_HPP

/**
 * @file MultiIntervalBatchWorker.hpp
 * @brief Background QThread worker for multi-interval batch inference.
 *
 * Runs SlotAssembler::runBatchRangeOffline sequentially over multiple frame
 * intervals on one worker thread with unified progress reporting.
 *
 * @see InferenceController
 * @see BatchInferenceWorker
 */

#include "Core/DeepLearningBindingData.hpp"     // SlotBindingData, OutputBindingData, StaticInputData
#include "Core/MediaOverrides.hpp"              // MediaOverrides
#include "Core/WriteReservation.hpp"            // WriteReservation

#include "CoreGeometry/ImageSize.hpp"            // ImageSize

#include <QThread>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class DataManager;
class SlotAssembler;

/// Runs offline batch inference over multiple frame intervals on a QThread.
class MultiIntervalBatchWorker : public QThread {
    Q_OBJECT

public:
    /// @param assembler Non-owning pointer; caller retains ownership.
    /// @param dm Non-owning DataManager for encoding inputs and writing outputs.
    /// @param reservation Shared buffer for progressive result delivery.
    /// @param cancel_flag Shared flag polled between batches; set by InferenceController::cancel().
    /// @param parent Optional QObject parent.
    MultiIntervalBatchWorker(
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
            QObject * parent = nullptr);

    /// Whether all intervals completed without error.
    [[nodiscard]] bool success() const;

    /// Non-empty when success() is false.
    [[nodiscard]] std::string const & errorMessage() const;

signals:
    /// Emitted during batch inference; progress is cumulative across intervals.
    void progressChanged(int current, int total);

protected:
    void run() override;

private:
    SlotAssembler * _assembler;
    DataManager * _dm;
    MediaOverrides _media_overrides;
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

#endif// MULTI_INTERVAL_BATCH_WORKER_HPP
