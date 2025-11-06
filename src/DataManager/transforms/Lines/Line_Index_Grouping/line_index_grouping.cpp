#include "line_index_grouping.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "Lines/Line_Data.hpp"
#include "transforms/utils/index_grouping.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <iostream>

std::shared_ptr<LineData> lineIndexGrouping(std::shared_ptr<LineData> line_data,
                                            LineIndexGroupingParameters const * params) {
    // No-op progress callback - delegate to version with callback
    return ::lineIndexGrouping(std::move(line_data), params, [](int) {/* no progress reporting */});
}

std::shared_ptr<LineData> lineIndexGrouping(std::shared_ptr<LineData> line_data,
                                            LineIndexGroupingParameters const * params,
                                            ProgressCallback const & progressCallback) {
    if (!line_data || !params) {
        std::cerr << "lineIndexGrouping: Invalid input (null line_data or params)" << std::endl;
        return line_data;
    }

    // Check if group manager is valid (required for grouping operations)
    if (!params->hasValidGroupManager()) {
        std::cerr << "lineIndexGrouping: EntityGroupManager is null. Cannot perform grouping." << std::endl;
        return line_data;
    }

    auto group_manager = params->getGroupManager();

    // Clear existing groups if requested
    if (params->clear_existing_groups) {
        auto all_groups = group_manager->getAllGroupIds();
        for (auto group_id: all_groups) {
            group_manager->deleteGroup(group_id);
        }
    }

    // Get all time frames with data
    auto times_view = line_data->getTimesWithData();
    std::vector<TimeFrameIndex> const all_times(times_view.begin(), times_view.end());
    
    if (all_times.empty()) {
        std::cerr << "lineIndexGrouping: No data found in LineData" << std::endl;
        return line_data;
    }

    // Report progress at start
    progressCallback(0);

    // Get the internal data structure from LineData
    // GetAllLineEntriesAsRange() provides a zero-copy view of the data
    auto line_entries_range = line_data->GetAllLineEntriesAsRange();

    // Build a map structure for the templated function
    std::map<TimeFrameIndex, std::vector<LineEntry>> data_map;
    for (auto const & pair: line_entries_range) {
        std::vector<LineEntry> entry_vec;
        entry_vec.reserve(pair.entries.size());
        
        for (auto const & entry: pair.entries) {
            entry_vec.push_back(entry);
        }
        
        data_map[pair.time] = std::move(entry_vec);
    }

    // Call the templated grouping function
    std::size_t const num_groups = ::groupByIndex<std::map<TimeFrameIndex, std::vector<LineEntry>>, LineEntry>(
        data_map,
        group_manager,
        params->group_name_prefix,
        params->group_description_template);

    // Report progress at completion
    progressCallback(100);

    std::cout << "lineIndexGrouping: Created " << num_groups << " groups" << std::endl;

    return line_data;
}

// LineIndexGroupingOperation implementation

std::string LineIndexGroupingOperation::getName() const {
    return "Group Lines by Index";
}

std::type_index LineIndexGroupingOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineIndexGroupingOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineIndexGroupingOperation::getDefaultParameters() const {
    // Return default parameters with null group manager
    // The group manager must be set before execution
    return std::make_unique<LineIndexGroupingParameters>();
}

DataTypeVariant LineIndexGroupingOperation::execute(DataTypeVariant const & dataVariant,
                                                    TransformParametersBase const * transformParameters) {
    // Delegate to the version with progress callback (no progress reporting needed for synchronous call)
    return execute(dataVariant, transformParameters, [](int) {/* no-op */});
}

DataTypeVariant LineIndexGroupingOperation::execute(DataTypeVariant const & dataVariant,
                                                    TransformParametersBase const * transformParameters,
                                                    ProgressCallback progressCallback) {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        std::cerr << "LineIndexGroupingOperation: Invalid input type" << std::endl;
        return dataVariant;
    }

    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);

    auto const * params = dynamic_cast<LineIndexGroupingParameters const *>(transformParameters);
    if (!params) {
        std::cerr << "LineIndexGroupingOperation: Invalid parameters type" << std::endl;
        return dataVariant;
    }

    auto result = ::lineIndexGrouping(line_data, params, progressCallback);
    return DataTypeVariant{result};
}
