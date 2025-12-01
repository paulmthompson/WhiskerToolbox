#ifndef WHISKERTOOLBOX_V2_COMPUTE_CONTEXT_HPP
#define WHISKERTOOLBOX_V2_COMPUTE_CONTEXT_HPP

#include <functional>
#include <string>

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Context for transform execution
 * 
 * Provides additional information and services during transform execution:
 * - Progress reporting
 * - Cancellation checking
 * - Logging
 * - Provenance tracking
 */
struct ComputeContext {
    using ProgressCallback = std::function<void(int progress)>;
    using CancellationCheck = std::function<bool()>;
    using Logger = std::function<void(std::string const & message)>;

    ProgressCallback progress;
    CancellationCheck is_cancelled;
    Logger log;

    // Provenance tracking (optional)
    void * provenance_tracker = nullptr;

    // Helper methods
    void reportProgress(int p) const {
        if (progress) { progress(p); }
    }

    bool shouldCancel() const {
        return is_cancelled && is_cancelled();
    }

    void logMessage(std::string const & msg) const {
        if (log) { log(msg); }
    }
};

} // namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_COMPUTE_CONTEXT_HPP