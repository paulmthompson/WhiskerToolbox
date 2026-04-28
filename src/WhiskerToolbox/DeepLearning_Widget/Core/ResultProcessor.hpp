/**
 * @file ResultProcessor.hpp
 * @brief Merges batch inference results into DataManager and accumulates
 *        feature vectors for TensorData output.
 *
 * Owns the merge timer lifecycle and processes FrameResult entries drained
 * from WriteReservation. Geometry (Mask, Point, Line) is written immediately;
 * feature vectors are accumulated and flushed to TensorData on demand.
 *
 * @see InferenceController
 * @see WriteReservation
 */

#ifndef DEEP_LEARNING_RESULT_PROCESSOR_HPP
#define DEEP_LEARNING_RESULT_PROCESSOR_HPP

#include "Core/BatchInferenceResult.hpp"

#include <QObject>

#include <map>
#include <memory>
#include <string>
#include <vector>

class DataManager;
class DeepLearningState;
class WriteReservation;

/**
 * @brief Merges decoded inference results into DataManager and flushes feature vectors.
 */
class ResultProcessor : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor for ResultProcessor.
     * 
     * @param dm Shared DataManager for writing outputs.
     * @param state Shared state (used for context; bindings come from results).
     * @param parent Optional QObject parent.
     */
    explicit ResultProcessor(std::shared_ptr<DataManager> dm,
                             std::shared_ptr<DeepLearningState> state,
                             QObject * parent = nullptr);

    ~ResultProcessor() override;

    /**
     * @brief Process a batch of frame results: write geometry immediately, accumulate
     *        feature vectors for later flush.
     * 
     * @param results Decoded outputs from batch inference.
     */
    void acceptResults(std::vector<FrameResult> results);

    /**
     * @brief Write accumulated feature vectors to TensorData and clear the buffer.
     */
    void flushFeatureVectors();

    /**
     * @brief Clear accumulated feature vectors (call at batch start).
     */
    void clear();

    /**
     * @brief Set the reservation to drain on timer ticks. Call before startMergeTimer().
     * 
     * @param reservation Shared reservation; may be null to clear.
     */
    void setReservation(std::shared_ptr<WriteReservation> reservation);

    /**
     * @brief Start periodic merge timer (200 ms). Drains reservation on each tick.
     */
    void startMergeTimer();

    /**
     * @brief Stop merge timer and perform final drain.
     */
    void stopMergeTimer();

signals:
    /**
     * @brief Emitted after processing results that wrote to geometry data.
     * 
     * @param count Number of data keys that received updates.
     */
    void resultsWritten(int count);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#endif// DEEP_LEARNING_RESULT_PROCESSOR_HPP
