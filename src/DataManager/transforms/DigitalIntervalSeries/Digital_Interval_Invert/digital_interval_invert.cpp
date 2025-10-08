#include "digital_interval_invert.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

std::shared_ptr<DigitalIntervalSeries> invert_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        InvertParams const & invertParams) {
    return invert_intervals(digital_interval_series, invertParams, [](int) {});
}

std::shared_ptr<DigitalIntervalSeries> invert_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        InvertParams const & invertParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!digital_interval_series) {
        std::cerr << "invert_intervals: Input DigitalIntervalSeries is null" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();
    }

    auto const & intervals = digital_interval_series->getDigitalIntervalSeries();

    if (intervals.empty()) {
        if (invertParams.domainType == DomainType::Bounded) {
            // If bounded and no intervals, return the entire domain as one interval
            std::vector<Interval> result_intervals;
            result_intervals.emplace_back(static_cast<int64_t>(invertParams.boundStart),
                                        static_cast<int64_t>(invertParams.boundEnd));
            return std::make_shared<DigitalIntervalSeries>(result_intervals);
        } else {
            // If unbounded and no intervals, return empty
            return std::make_shared<DigitalIntervalSeries>();
        }
    }

    if (progressCallback) {
        progressCallback(10);
    }

    // Create a copy and sort by start time to ensure proper ordering
    std::vector<Interval> sorted_intervals = intervals;
    std::sort(sorted_intervals.begin(), sorted_intervals.end(), [](Interval const & a, Interval const & b) {
        return a.start < b.start;
    });

    if (progressCallback) {
        progressCallback(20);
    }

    std::vector<Interval> inverted_intervals;

    // Handle bounded domain - add interval before first interval if needed
    if (invertParams.domainType == DomainType::Bounded) {
        auto const bound_start = static_cast<int64_t>(invertParams.boundStart);
        auto const bound_end = static_cast<int64_t>(invertParams.boundEnd);

        if (sorted_intervals[0].start > bound_start) {
            inverted_intervals.emplace_back(bound_start, sorted_intervals[0].start);
        }
    }

    if (progressCallback) {
        progressCallback(40);
    }

    // Process gaps between intervals
    for (size_t i = 0; i < sorted_intervals.size() - 1; ++i) {
        if (progressCallback && i % 100 == 0) {
            int const progress = 40 + static_cast<int>((i * 40) / sorted_intervals.size());
            progressCallback(progress);
        }

        Interval const & current = sorted_intervals[i];
        Interval const & next = sorted_intervals[i + 1];

        // Check if there's a gap between current interval end and next interval start
        if (current.end < next.start) {
            inverted_intervals.emplace_back(current.end, next.start);
        }
    }

    if (progressCallback) {
        progressCallback(80);
    }

    // Handle bounded domain - add interval after last interval if needed
    if (invertParams.domainType == DomainType::Bounded) {
        auto const bound_end = static_cast<int64_t>(invertParams.boundEnd);

        if (sorted_intervals.back().end < bound_end) {
            inverted_intervals.emplace_back(sorted_intervals.back().end, bound_end);
        }
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return std::make_shared<DigitalIntervalSeries>(inverted_intervals);
}

///////////////////////////////////////////////////////////////////////////////

std::string InvertIntervalOperation::getName() const {
    return "Invert Intervals";
}

std::type_index InvertIntervalOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<DigitalIntervalSeries>);
}

bool InvertIntervalOperation::canApply(DataTypeVariant const & dataVariant) const {
    if (!std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(dataVariant)) {
        return false;
    }

    auto const * ptr_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&dataVariant);
    return ptr_ptr && *ptr_ptr;
}

std::unique_ptr<TransformParametersBase> InvertIntervalOperation::getDefaultParameters() const {
    return std::make_unique<InvertParams>();
}

DataTypeVariant InvertIntervalOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant InvertIntervalOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "InvertIntervalOperation::execute: Invalid input data variant" << std::endl;
        return {};
    }

    DigitalIntervalSeries const * digital_raw_ptr = (*ptr_ptr).get();

    InvertParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams =
                dynamic_cast<InvertParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "InvertIntervalOperation::execute: Incompatible parameter type, using defaults" << std::endl;
        }
    }

    std::shared_ptr<DigitalIntervalSeries> result = invert_intervals(
            digital_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "InvertIntervalOperation::execute: Interval inversion failed" << std::endl;
        return {};
    }

    return result;
}
