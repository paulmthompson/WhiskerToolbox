#include "digital_interval_group.hpp"

#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "TimeFrame/interval_data.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

std::shared_ptr<DigitalIntervalSeries> group_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        GroupParams const & groupParams) {
    return group_intervals(digital_interval_series, groupParams, [](int) {});
}

std::shared_ptr<DigitalIntervalSeries> group_intervals(
        DigitalIntervalSeries const * digital_interval_series,
        GroupParams const & groupParams,
        ProgressCallback progressCallback) {

    // Input validation
    if (!digital_interval_series) {
        std::cerr << "group_intervals: Input DigitalIntervalSeries is null" << std::endl;
        return std::make_shared<DigitalIntervalSeries>();;
    }

    auto const & intervals = digital_interval_series->view();

    if (intervals.empty()) {
        return std::make_shared<DigitalIntervalSeries>();;
    }

    if (progressCallback) {
        progressCallback(10);
    }

    // Create a copy and sort by start time to ensure proper ordering
    std::vector<Interval> sorted_intervals;
    for (auto const & interval_with_id : intervals) {
        sorted_intervals.push_back(interval_with_id.value());
    }
    std::sort(sorted_intervals.begin(), sorted_intervals.end(), [](Interval const & a, Interval const & b) {
        return a.start < b.start;
    });

    if (progressCallback) {
        progressCallback(20);
    }

    std::vector<Interval> grouped_intervals;
    auto const max_spacing = static_cast<int64_t>(groupParams.maxSpacing);

    // Start with the first interval
    if (!sorted_intervals.empty()) {
        Interval current_group = sorted_intervals[0];

        for (size_t i = 1; i < sorted_intervals.size(); ++i) {
            if (progressCallback && i % 100 == 0) {
                int const progress = 20 + static_cast<int>((i * 60) / sorted_intervals.size());
                progressCallback(progress);
            }

            Interval const & next_interval = sorted_intervals[i];

            // Calculate gap between current group end and next interval start
            int64_t const gap = next_interval.start - current_group.end - 1;

            if (gap <= max_spacing) {
                // Group them by extending the current group to include the next interval
                current_group.end = next_interval.end;
            } else {
                // Gap is too large, finalize current group and start a new one
                grouped_intervals.push_back(current_group);
                current_group = next_interval;
            }
        }

        // Don't forget the last group
        grouped_intervals.push_back(current_group);
    }

    if (progressCallback) {
        progressCallback(100);
    }

    return std::make_shared<DigitalIntervalSeries>(grouped_intervals);

}

///////////////////////////////////////////////////////////////////////////////

std::string GroupOperation::getName() const {
    return "Group Intervals";
}

std::type_index GroupOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<DigitalIntervalSeries>);
}

bool GroupOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<DigitalIntervalSeries>(dataVariant);
}

std::unique_ptr<TransformParametersBase> GroupOperation::getDefaultParameters() const {
    return std::make_unique<GroupParams>();
}

DataTypeVariant GroupOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

DataTypeVariant GroupOperation::execute(
        DataTypeVariant const & dataVariant,
        TransformParametersBase const * transformParameters,
        ProgressCallback progressCallback) {

    auto const * ptr_ptr = std::get_if<std::shared_ptr<DigitalIntervalSeries>>(&dataVariant);

    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "GroupOperation::execute: Invalid input data variant" << std::endl;
        return {};
    }

    DigitalIntervalSeries const * digital_raw_ptr = (*ptr_ptr).get();

    GroupParams currentParams;

    if (transformParameters != nullptr) {
        auto const * specificParams =
                dynamic_cast<GroupParams const *>(transformParameters);

        if (specificParams) {
            currentParams = *specificParams;
        } else {
            std::cerr << "GroupOperation::execute: Incompatible parameter type, using defaults" << std::endl;
        }
    }

    std::shared_ptr<DigitalIntervalSeries> result = group_intervals(
            digital_raw_ptr, currentParams, progressCallback);

    if (!result) {
        std::cerr << "GroupOperation::execute: Interval grouping failed" << std::endl;
        return {};
    }

    return result;
}