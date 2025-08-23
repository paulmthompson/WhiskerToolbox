#include "IntervalReductionComputer.h"

#include "utils/TableView/interfaces/IAnalogSource.h"

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
    , m_sourceName(m_source ? m_source->getName() : "null_source")
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

    // Get the list of intervals from the execution plan
    // This should also return the TimeFrame the intervals belong to
    // They way they can be converted.
    auto const & intervals = plan.getIntervals();
    auto destinationTimeFrame = plan.getTimeFrame();

    std::vector<double> results;
    results.reserve(intervals.size());

    for (auto const & interval: intervals) {
            
        auto sliceView = m_source->getDataInRange(
            interval.start, 
            interval.end, 
            destinationTimeFrame.get()
        );

         const float result = computeReduction(sliceView);

        results.emplace_back(result);
    }

    return results;

}

std::string IntervalReductionComputer::getSourceDependency() const {
    return m_sourceName;
}

float IntervalReductionComputer::computeReduction(std::span<const float> data) const {
    if (data.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
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

float IntervalReductionComputer::computeMean(std::span<const float> data) const {
    if (data.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    const float sum = std::accumulate(data.begin(), data.end(), 0.0f);
    return sum / static_cast<float>(data.size());
}

float IntervalReductionComputer::computeMax(std::span<const float> data) const {
    if (data.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return *std::max_element(data.begin(), data.end());
}

float IntervalReductionComputer::computeMin(std::span<const float> data) const {
    if (data.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    return *std::min_element(data.begin(), data.end());
}

float IntervalReductionComputer::computeStdDev(std::span<const float> data) const {
    if (data.empty()) {
        return std::numeric_limits<float>::quiet_NaN();
    }

    if (data.size() == 1) {
        return 0.0;
    }

    const float mean = computeMean(data);
    float variance = 0.0;

    for (const float value : data) {
        const float diff = value - mean;
        variance += diff * diff;
    }

    variance /= static_cast<float>(data.size() - 1); // Sample standard deviation
    return std::sqrt(variance);
}

float IntervalReductionComputer::computeSum(std::span<const float> data) const {
    if (data.empty()) {
        return 0.0f;
    }

    return std::accumulate(data.begin(), data.end(), 0.0f);
}

float IntervalReductionComputer::computeCount(std::span<const float> data) const {
    return static_cast<float>(data.size());
}
