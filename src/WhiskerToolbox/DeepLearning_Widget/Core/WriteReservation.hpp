/**
 * @file WriteReservation.hpp
 * @brief Thread-safe buffer for progressive delivery of inference results.
 *
 * Implements the write reservation pattern from the Concurrency Architecture:
 * the worker thread pushes decoded FrameResult entries into the buffer, and
 * the main thread periodically drains them for writing into DataManager.
 *
 * The mutex protects only the pending-results vector — DataManager and its
 * data objects are never touched under the lock.
 */

#ifndef WRITE_RESERVATION_HPP
#define WRITE_RESERVATION_HPP

#include "BatchInferenceResult.hpp"

#include <mutex>
#include <vector>

/**
 * @brief Thread-safe accumulation buffer for progressive result delivery.
 * 
 * The worker thread calls push() after decoding each frame's outputs.
 * The main thread calls drain() on a timer to move accumulated results
 * into DataManager in bulk.
 */
class WriteReservation {
public:
    /**
     * @brief Push decoded results from the worker thread.
     * 
     * @param results Decoded outputs for one frame (may contain multiple bindings)
     */
    void push(std::vector<FrameResult> results) {
        std::lock_guard const lock(_mutex);
        _pending.insert(
                _pending.end(),
                std::make_move_iterator(results.begin()),
                std::make_move_iterator(results.end()));
    }

    /**
     * @brief Drain all pending results (called from main thread).
     * 
     * @returns An empty vector if nothing is pending.
     */
    [[nodiscard]] std::vector<FrameResult> drain() {
        std::lock_guard const lock(_mutex);
        std::vector<FrameResult> drained;
        drained.swap(_pending);
        return drained;
    }

    /**
     * @brief Check if there are pending results without draining.
     */
    [[nodiscard]] bool hasPending() const {
        std::lock_guard const lock(_mutex);
        return !_pending.empty();
    }

private:
    mutable std::mutex _mutex;
    std::vector<FrameResult> _pending;
};

#endif// WRITE_RESERVATION_HPP
