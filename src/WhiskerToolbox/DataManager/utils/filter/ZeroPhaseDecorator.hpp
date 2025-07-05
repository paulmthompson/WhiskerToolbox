#ifndef ZERO_PHASE_DECORATOR_HPP
#define ZERO_PHASE_DECORATOR_HPP

#include "IFilter.hpp"

#include <memory>
#include <algorithm>
#include <vector>

/**
 * @brief Decorator that applies zero-phase filtering to any filter
 * 
 * This decorator implements zero-phase filtering by applying the wrapped filter
 * in both forward and reverse directions. This eliminates phase distortion
 * but doubles the effective filter order.
 */
class ZeroPhaseDecorator : public IFilter {
private:
    std::unique_ptr<IFilter> wrapped_filter_;

public:
    /**
     * @brief Construct a zero-phase decorator around an existing filter
     * 
     * @param filter The filter to wrap (takes ownership)
     */
    explicit ZeroPhaseDecorator(std::unique_ptr<IFilter> filter) 
        : wrapped_filter_(std::move(filter)) {
        if (!wrapped_filter_) {
            throw std::invalid_argument("Wrapped filter cannot be null");
        }
    }

    void process(std::span<float> data) override {
        if (data.empty()) {
            return;
        }

        // Create a working copy for the forward pass
        std::vector<float> working_data(data.begin(), data.end());

        // Forward pass
        wrapped_filter_->reset();
        wrapped_filter_->process(working_data);

        // Reverse the data for backward pass
        std::reverse(working_data.begin(), working_data.end());

        // Backward pass
        wrapped_filter_->reset();
        wrapped_filter_->process(working_data);

        // Reverse back to correct time order and copy to output
        std::reverse(working_data.begin(), working_data.end());
        std::copy(working_data.begin(), working_data.end(), data.begin());
    }

    void reset() override {
        wrapped_filter_->reset();
    }

    std::string getName() const override {
        return "ZeroPhase(" + wrapped_filter_->getName() + ")";
    }

    /**
     * @brief Get access to the wrapped filter
     * 
     * @return Const reference to the wrapped filter
     */
    IFilter const& getWrappedFilter() const {
        return *wrapped_filter_;
    }
};

#endif // ZERO_PHASE_DECORATOR_HPP
