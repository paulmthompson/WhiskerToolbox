#ifndef BATCH_INFERENCE_WORKER_HPP
#define BATCH_INFERENCE_WORKER_HPP

/**
 * @file BatchInferenceWorker.hpp
 * @brief Background QThread worker for single-range batch inference.
 *
 * Runs SlotAssembler::runBatchRangeOffline on a worker thread and reports
 * progress via Qt signals. Used by InferenceController for runBatch().
 *
 * @see InferenceController
 * @see MultiIntervalBatchWorker
 */

#include "DeepLearning/bindings/DeepLearningBindingData.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"
#include "Core/MediaOverrides.hpp"         // MediaOverrides
#include "Inference/WriteReservation.hpp"  // WriteReservation

#include "CoreGeometry/ImageSize.hpp"// ImageSize

#include <QThread>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class SlotAssembler;

/**
 * @brief Runs offline batch inference for one contiguous frame range on a QThread.
 */
class BatchInferenceWorker : public QThread {
    Q_OBJECT

public:
    /**
     * @brief Constructor for BatchInferenceWorker.
     *
     * @param assembler Non-owning pointer; caller retains ownership.
     * @param dm Non-owning DataManager for encoding inputs and writing outputs.
     * @param reservation Shared buffer for progressive result delivery.
     * @param cancel_flag Shared flag polled between batches; set by InferenceController::cancel().
     * @param parent Optional QObject parent.
     */
    BatchInferenceWorker(
            SlotAssembler * assembler,
            DataManager * dm,
            MediaOverrides media_overrides,
            std::vector<SlotBindingData> input_bindings,
            std::vector<StaticInputData> static_inputs,
            std::vector<OutputBindingData> output_bindings,
            int start_frame,
            int end_frame,
            ImageSize source_image_size,
            int batch_size,
            std::shared_ptr<WriteReservation> reservation,
            std::shared_ptr<std::atomic<bool>> cancel_flag,
            QObject * parent = nullptr);

    /**
     * @brief Whether the batch run completed without error.
     */
    [[nodiscard]] bool success() const;

    /**
     * @brief Non-empty when success() is false.
     */
    [[nodiscard]] std::string const & errorMessage() const;

signals:
    /**
     * @brief Emitted during batch inference; may originate from the worker thread.
     */
    void progressChanged(int current, int total);

protected:
    /**
     * @brief Executes batch inference on the worker thread.
     */
    void run() override;

private:
    SlotAssembler * _assembler;
    DataManager * _dm;
    MediaOverrides _media_overrides;
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

#endif// BATCH_INFERENCE_WORKER_HPP
