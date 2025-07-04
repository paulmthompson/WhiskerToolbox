#ifndef IFILTER_HPP
#define IFILTER_HPP

#include "TimeFrame.hpp"
#include <span>
#include <vector>

class AnalogTimeSeries;

/**
 * @brief Abstract interface for all filters
 * 
 * This interface defines the contract for filters that process analog time series data.
 * The virtual function call overhead occurs only once per processing call, not per sample.
 */
class IFilter {
public:
    virtual ~IFilter() = default;

    /**
     * @brief Process analog time series data in-place
     * 
     * This method processes the entire data array. The implementation should handle
     * the sample-by-sample loop internally to avoid virtual function call overhead
     * per sample.
     * 
     * @param data Span of data to process in-place
     */
    virtual void process(std::span<float> data) = 0;

    /**
     * @brief Reset the filter's internal state
     * 
     * This clears any internal filter state (delay lines, etc.) so the filter
     * can be reused for a new data sequence.
     */
    virtual void reset() = 0;

    /**
     * @brief Get a descriptive name for this filter
     * 
     * @return String describing the filter (e.g., "Butterworth Lowpass Order 4")
     */
    virtual std::string getName() const = 0;
};

#endif // IFILTER_HPP
