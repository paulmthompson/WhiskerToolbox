#include "IntervalReductionComputer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

IntervalReductionComputer::IntervalReductionComputer(std::shared_ptr<IAnalogSource> source, 
                                                   ReductionType reduction)
    : IColumnComputer()
    , m_source(std::move(source))
    , m_reduction(reduction)
    , m_sourceName("default_source")
{
    if (!m_source) {
        throw std::invalid_argument("IAnalogSource cannot be null");
    }
}

IntervalReductionComputer::IntervalReductionComputer(std::shared_ptr<IAnalogSource> source, 
                                                   ReductionType reduction,
                                                   std::string sourceName)
    : IColumnComputer()
    , m_source(std::move(source))
    , m_reduction(reduction)
    , m_sourceName(std::move(sourceName))
{
    if (!m_source) {
        throw std::invalid_argument("IAnalogSource cannot be null");
    }
}

std::vector<double> IntervalReductionComputer::compute(const ExecutionPlan& plan) const {
    if (!plan.hasIntervals()) {
        throw std::invalid_argument("ExecutionPlan must contain intervals for IntervalReductionComputer");
    }

    const auto& intervals = plan.getIntervals();
    std::vector<double> results;
    results.reserve(intervals.size());

    // Get the full data span once for efficiency
    auto fullDataSpan = m_source->getDataSpan();

    for (const auto& interval : intervals) {
        // Convert TimeFrameIndex to array indices
        auto startIdx = static_cast<size_t>(interval.start.getValue());
        auto endIdx = static_cast<size_t>(interval.end.getValue());

        // Bounds checking
        if (startIdx >= fullDataSpan.size() || endIdx >= fullDataSpan.size()) {
            // Handle out-of-bounds by using available data or NaN
            if (startIdx >= fullDataSpan.size()) {
                results.push_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }
            endIdx = fullDataSpan.size() - 1;
        }

        // Ensure start <= end
        if (startIdx > endIdx) {
            results.push_back(std::numeric_limits<double>::quiet_NaN());
            continue;
        }

        // Create span for this interval (inclusive range)
        auto intervalSpan = fullDataSpan.subspan(startIdx, endIdx - startIdx + 1);
        
        // Compute the reduction for this interval
        const double result = computeReduction(intervalSpan);
        results.push_back(result);
    }

    return results;
}

std::string IntervalReductionComputer::getSourceDependency() const {
    return m_sourceName;
}

double IntervalReductionComputer::computeReduction(std::span<const double> data) const {
    if (data.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    switch (m_reduction) {
        case ReductionType::Mean:
            return computeMean(data);
        case ReductionType::Max:
            return computeMax(data);
        case ReductionType::Min:
            return computeMin(data);
        case ReductionType::StdDev:
            return computeStdDev(data);
        case ReductionType::Sum:
            return computeSum(data);
        case ReductionType::Count:
            return computeCount(data);
        default:
            throw std::invalid_argument("Unknown reduction type");
    }
}

double IntervalReductionComputer::computeMean(std::span<const double> data) const {
    if (data.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return sum / static_cast<double>(data.size());
}

double IntervalReductionComputer::computeMax(std::span<const double> data) const {
    if (data.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return *std::max_element(data.begin(), data.end());
}

double IntervalReductionComputer::computeMin(std::span<const double> data) const {
    if (data.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return *std::min_element(data.begin(), data.end());
}

double IntervalReductionComputer::computeStdDev(std::span<const double> data) const {
    if (data.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (data.size() == 1) {
        return 0.0;
    }

    const double mean = computeMean(data);
    double variance = 0.0;

    for (const double value : data) {
        const double diff = value - mean;
        variance += diff * diff;
    }

    variance /= static_cast<double>(data.size() - 1); // Sample standard deviation
    return std::sqrt(variance);
}

double IntervalReductionComputer::computeSum(std::span<const double> data) const {
    if (data.empty()) {
        return 0.0;
    }

    return std::accumulate(data.begin(), data.end(), 0.0);
}

double IntervalReductionComputer::computeCount(std::span<const double> data) const {
    return static_cast<double>(data.size());
}
