#ifndef DEEP_LEARNING_INFERENCE_CONTROLLER_HPP
#define DEEP_LEARNING_INFERENCE_CONTROLLER_HPP

/**
 * @file InferenceController.hpp
 * @brief Orchestrates deep learning inference (single frame, batch, recurrent).
 *
 * Owns the worker thread lifecycle for batch inference and coordinates
 * result merging. The properties widget delegates run actions to this
 * controller and forwards progress signals to the view.
 *
 * @see DeepLearningPropertiesWidget
 * @see DeepLearningViewWidget
 */

#include "Core/SlotAssembler.hpp"

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class DataManager;
class DeepLearningState;

/// Orchestrates inference runs and owns the batch worker thread.
class InferenceController : public QObject {
    Q_OBJECT

public:
    /// @param assembler Non-owning pointer; caller retains ownership.
    /// @param dm Shared DataManager for encoding inputs and writing outputs.
    /// @param state Shared state holding bindings and parameters.
    /// @param parent Optional QObject parent.
    explicit InferenceController(SlotAssembler * assembler,
                                 std::shared_ptr<DataManager> dm,
                                 std::shared_ptr<DeepLearningState> state,
                                 QObject * parent = nullptr);

    ~InferenceController() override;

    /// Whether a batch inference is currently running.
    [[nodiscard]] bool isRunning() const;

public slots:
    /// Run single-frame inference at the given frame index.
    /// @param frame Frame index to process.
    ///
    /// @pre frame >= 0; a negative value is silently clamped to 0 by
    ///      computeEncodingFrame() during input assembly, causing the wrong
    ///      frame to be encoded without any error signal
    ///      (enforcement: none) [IMPORTANT]
    void runSingleFrame(int frame);

    /// Run independent (non-recurrent) batch inference over [start, end].
    /// @param start First frame (inclusive).
    /// @param end Last frame (inclusive).
    /// @param batch_size Number of frames per forward pass (>= 1).
    ///
    /// @pre start >= 0; a negative start frame is silently clamped to 0 by
    ///      computeEncodingFrame() during input assembly, causing the wrong
    ///      frames to be encoded without any error signal
    ///      (enforcement: none) [IMPORTANT]
    void runBatch(int start, int end, int batch_size);

    /// Run independent batch inference over multiple intervals.
    /// Progress reports the cumulative frame count across all intervals.
    /// @param intervals Vector of (start, end) frame ranges (inclusive).
    /// @param batch_size Number of frames per forward pass (>= 1).
    ///
    /// @pre Each interval's start value must be >= 0; a negative start is
    ///      silently clamped to 0 by computeEncodingFrame() during input
    ///      assembly, causing the wrong frames to be encoded without any error
    ///      signal (enforcement: none) [IMPORTANT]
    void runBatchIntervals(std::vector<std::pair<int64_t, int64_t>> intervals,
                           int batch_size);

    /// Run sequential recurrent inference over a frame range.
    /// @param start First frame index.
    /// @param frame_count Number of frames to process.
    ///
    /// @pre start >= 0; a negative start frame is silently clamped to 0 by
    ///      computeEncodingFrame() during input assembly, causing the wrong
    ///      frames to be encoded without any error signal
    ///      (enforcement: none) [IMPORTANT]
    /// @pre frame_count >= 1; a value of 0 or negative causes the frame loop
    ///      to silently produce no output and emit batchFinished(true)
    ///      (enforcement: none) [LOW]
    void runRecurrentSequence(int start, int frame_count);

    /// Request cancellation of the current batch inference.
    void cancel();

signals:
    /// Emitted during batch inference; Qt queued from worker thread.
    void batchProgressChanged(int current, int total);

    /// Emitted during recurrent inference.
    void recurrentProgressChanged(int current, int total);

    /// Emitted when batch inference completes (success or failure).
    /// @param success Whether inference completed without error.
    /// @param error_message Non-empty when success is false.
    void batchFinished(bool success, QString const & error_message);

    /// Emitted when running state changes.
    void runningChanged(bool running);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif// DEEP_LEARNING_INFERENCE_CONTROLLER_HPP
