#ifndef PIPELINE_OUTPUT_BUILDER_HPP
#define PIPELINE_OUTPUT_BUILDER_HPP

#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <vector>

namespace WhiskerToolbox::Transforms::V2 {


// ============================================================================
// Helper for Adding Elements to Different Container Types
// ============================================================================

// Helper for static_assert in else branch
template<typename>
inline constexpr bool always_false = false;

/**
 * @brief Add element to output container, handling different container APIs
 * 
 * Different containers have different methods:
 * - RaggedTimeSeries<T>: addAtTime(time, T, notify)
 * - RaggedAnalogTimeSeries: appendAtTime(time, std::vector<float>, notify)
 * 
 * This helper uses compile-time checks to call the right method.
 */
template<typename Container, typename Element>
void addElementToContainer(Container & container, TimeFrameIndex time, Element && element) {
    // Check if container has appendAtTime (like RaggedAnalogTimeSeries)
    if constexpr (requires { container.appendAtTime(time, std::vector<Element>{}, NotifyObservers::No); }) {
        // Wrap single element in vector for appendAtTime
        container.appendAtTime(time, std::vector<Element>{std::forward<Element>(element)}, NotifyObservers::No);
    }
    // Check if container has addAtTime for single elements (like RaggedTimeSeries<T>)
    else if constexpr (requires { container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No); }) {
        container.addAtTime(time, std::forward<Element>(element), NotifyObservers::No);
    } else {
        static_assert(always_false<Container>, "Container type does not support addAtTime or appendAtTime");
    }
}

// ============================================================================
// Pipeline Output Builder
// ============================================================================

/**
 * @brief Helper to build output containers efficiently
 * 
 * Handles both incremental addition (RaggedTimeSeries) and batch loading (AnalogTimeSeries).
 */
template<typename Container, typename Element>
class PipelineOutputBuilder {
public:
    PipelineOutputBuilder(std::shared_ptr<TimeFrame> tf)
        : tf_(tf) {
        if constexpr (use_incremental) {
            container_ = std::make_shared<Container>();
            if constexpr (requires { container_->setTimeFrame(tf); }) {
                container_->setTimeFrame(tf);
            }
        }
    }

    void add(TimeFrameIndex time, Element element) {
        if constexpr (use_incremental) {
            addElementToContainer(*container_, time, std::move(element));
        } else {
            times_.push_back(time);
            values_.push_back(std::move(element));
        }
    }

    std::shared_ptr<Container> finalize() {
        if constexpr (use_incremental) {
            return container_;
        } else {
            auto container = std::make_shared<Container>(std::move(values_),
                                                         std::move(times_));
            if constexpr (requires { container->setTimeFrame(tf_); }) {
                container->setTimeFrame(tf_);
            }
            return container;
        }
    }

private:
    // Detect incremental capabilities
    static constexpr bool has_add_at_time = requires(Container & c, TimeFrameIndex t, Element e) {
        c.addAtTime(t, e, NotifyObservers::No);
    };

    static constexpr bool has_append_at_time = requires(Container & c, TimeFrameIndex t, std::vector<Element> v) {
        c.appendAtTime(t, v, NotifyObservers::No);
    };

    // Prefer incremental if available, otherwise fallback to batch
    static constexpr bool use_incremental = has_add_at_time || has_append_at_time;

    std::shared_ptr<Container> container_;
    std::shared_ptr<TimeFrame> tf_;

    std::vector<TimeFrameIndex> times_;
    std::vector<Element> values_;
};


} // namespace WhiskerToolbox::Transforms::V2


#endif // PIPELINE_OUTPUT_BUILDER_HPP