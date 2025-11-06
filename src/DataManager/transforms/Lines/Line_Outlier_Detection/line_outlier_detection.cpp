#include "line_outlier_detection.hpp"

#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "Lines/Line_Data.hpp"
#include "StateEstimation/Common.hpp"
#include "StateEstimation/Cost/CostFunctions.hpp"
#include "StateEstimation/DataAdapter.hpp"
#include "StateEstimation/Features/CompositeFeatureExtractor.hpp"
#include "StateEstimation/Features/IFeatureExtractor.hpp"
#include "StateEstimation/Features/LineCentroidExtractor.hpp"
#include "StateEstimation/Features/LineLengthExtractor.hpp"
#include "StateEstimation/Filter/Kalman/KalmanFilter.hpp"
#include "StateEstimation/Filter/Kalman/KalmanMatrixBuilder.hpp"
#include "StateEstimation/OutlierDetection.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <ranges>
#include <set>

namespace {
/**
 * @brief Convert flattened data source to legacy tuple format for OutlierDetection
 */
template<typename DataType>
std::vector<std::tuple<DataType, EntityId, TimeFrameIndex>>
convertToLegacyFormat(auto const & flattened_range) {
    std::vector<std::tuple<DataType, EntityId, TimeFrameIndex>> result;

    for (auto const & item: flattened_range) {
        result.emplace_back(item.data, item.entity_id, item.time);
    }

    return result;
}
}// anonymous namespace

std::shared_ptr<LineData> lineOutlierDetection(std::shared_ptr<LineData> line_data,
                                               LineOutlierDetectionParameters const * params) {
    // No-op progress callback - delegates to overload with progress reporting
    StateEstimation::ProgressCallback no_op = [](int) { /* No progress reporting */ };
    return ::lineOutlierDetection(std::move(line_data), params, no_op);
}

std::shared_ptr<LineData> lineOutlierDetection(std::shared_ptr<LineData> line_data,
                                               LineOutlierDetectionParameters const * params,
                                               StateEstimation::ProgressCallback const & progressCallback) {
    if (!line_data || !params) {
        std::cerr << "Error: null line_data or params" << std::endl;
        return line_data;
    }

    // Check if group manager is valid (required for grouping operations)
    if (!params->hasValidGroupManager()) {
        std::cerr << "Error: EntityGroupManager is null in parameters" << std::endl;
        return line_data;
    }

    using namespace StateEstimation;

    auto group_manager = params->getGroupManager();

    // Get all time frames with data
    auto times_view = line_data->getTimesWithData();
    std::vector<TimeFrameIndex> all_times(times_view.begin(), times_view.end());
    if (all_times.empty()) {
        std::cerr << "Error: no time frames with data" << std::endl;
        return line_data;
    }

    std::ranges::sort(all_times);
    TimeFrameIndex start_frame = all_times.front();
    TimeFrameIndex end_frame = all_times.back();

    if (params->verbose_output) {
        std::cout << "Processing frames "
                  << start_frame.getValue() << " to " << end_frame.getValue() << std::endl;
    }

    // Get natural iterator from LineData and flatten to individual items
    auto line_entries_range = line_data->GetAllLineEntriesAsRange();
    auto flattened_data = StateEstimation::flattenLineData(line_entries_range);

    // Convert to legacy tuple format for OutlierDetection
    auto data_source = convertToLegacyFormat<Line2D>(flattened_data);

    if (params->verbose_output) {
        std::cout << "Total line items: " << data_source.size() << std::endl;
    }

    // Determine which groups to process
    std::vector<GroupId> groups_to_process;
    if (params->groups_to_process.empty()) {
        // Process all groups
        groups_to_process = group_manager->getAllGroupIds();
    } else {
        groups_to_process = params->groups_to_process;
    }

    if (params->verbose_output) {
        std::cout << "Initial groups to consider: " << groups_to_process.size() << std::endl;
        for (auto gid : groups_to_process) {
            auto desc = group_manager->getGroupDescriptor(gid);
            if (desc) {
                std::cout << "  Group " << gid << ": " << desc->name 
                         << " (" << desc->entity_count << " entities)" << std::endl;
            }
        }
    }

    // Filter to only groups that have entities in this LineData
    auto all_entity_ids = line_data->getAllEntityIds();
    std::set<EntityId> entity_set(all_entity_ids.begin(), all_entity_ids.end());

    if (params->verbose_output) {
        std::cout << "LineData has " << all_entity_ids.size() << " entity IDs" << std::endl;
    }

    std::vector<GroupId> valid_groups;
    for (auto group_id: groups_to_process) {
        auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
        bool has_entity = std::ranges::any_of(entities_in_group,
                                              [&](EntityId eid) { return entity_set.contains(eid); });
        if (has_entity) {
            valid_groups.push_back(group_id);
            if (params->verbose_output) {
                std::cout << "  Group " << group_id << " has entities in LineData" << std::endl;
            }
        } else if (params->verbose_output) {
            std::cout << "  Group " << group_id << " has NO entities in LineData" << std::endl;
        }
    }

    if (valid_groups.empty()) {
        std::cerr << "Warning: no valid groups with entities found in LineData" << std::endl;
        return line_data;
    }

    if (params->verbose_output) {
        std::cout << "Processing " << valid_groups.size() << " groups" << std::endl;
    }

    // Create composite feature extractor with centroid + length
    auto composite_extractor = std::make_unique<StateEstimation::CompositeFeatureExtractor<Line2D>>();
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineCentroidExtractor>());
    composite_extractor->addExtractor(std::make_unique<StateEstimation::LineLengthExtractor>());

    // Get metadata from all child extractors
    auto metadata_list = composite_extractor->getChildMetadata();

    if (params->verbose_output) {
        std::cout << "Feature extractors configured:" << std::endl;
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            auto const & meta = metadata_list[i];
            std::cout << "  [" << i << "] " << meta.name
                      << " (dim=" << meta.measurement_size
                      << ", state_dim=" << meta.state_size << ")" << std::endl;
        }
    }

    // Build Kalman filter matrices using metadata-driven approach with per-feature configuration
    StateEstimation::KalmanMatrixBuilder::PerFeatureConfig builder_config;
    builder_config.dt = params->dt;
    builder_config.process_noise_position = params->process_noise_position;
    builder_config.process_noise_velocity = params->process_noise_velocity;
    builder_config.measurement_noise = params->measurement_noise_position; // Default for kinematic features
    builder_config.static_noise_scale = params->process_noise_length / params->process_noise_position; // Relative scale for static features
    
    // Set per-feature measurement noise (length has different noise than position)
    builder_config.feature_measurement_noise["LineCentroid"] = params->measurement_noise_position;
    builder_config.feature_measurement_noise["LineLength"] = params->measurement_noise_length;

    // Build all matrices at once
    auto [F, H, Q, R] = StateEstimation::KalmanMatrixBuilder::buildAllMatricesFromMetadataPerFeature(
        metadata_list, builder_config);

    if (params->verbose_output) {
        std::cout << "Kalman filter dimensions: state=" << F.rows()
                  << ", measurement=" << H.rows() << std::endl;
    }

    // Create Kalman filter prototype
    auto filter_prototype = std::make_unique<StateEstimation::KalmanFilter>(F, H, Q, R);

    // Create cost function (Mahalanobis distance)
    auto cost_function = StateEstimation::createMahalanobisCostFunction(H, R);

    // Create OutlierDetection instance with MAD threshold from parameters
    StateEstimation::OutlierDetection<Line2D> outlier_detector(
            std::move(filter_prototype),
            std::move(composite_extractor),
            cost_function,
            params->mad_threshold,
            params->verbose_output);

    if (params->verbose_output) {
        std::cout << "Using MAD threshold: " << params->mad_threshold << std::endl;
    }

    // Process the data
    outlier_detector.process(
            data_source,
            *group_manager,
            start_frame,
            end_frame,
            progressCallback,
            valid_groups);

    // Rename the outlier group if a custom name was requested
    // OutlierDetection always creates a group named "outlier"
    if (params->outlier_group_name != "outlier") {
        // Find the "outlier" group created by OutlierDetection
        auto all_groups = group_manager->getAllGroupIds();
        for (auto gid : all_groups) {
            auto desc = group_manager->getGroupDescriptor(gid);
            if (desc && desc->name == "outlier") {
                // Rename it to the desired name
                group_manager->updateGroup(gid, params->outlier_group_name, desc->description);
                if (params->verbose_output) {
                    std::cout << "Renamed outlier group to: " << params->outlier_group_name << std::endl;
                }
                break;
            }
        }
    }

    // Notify observers of group changes (critical for UI updates)
    group_manager->notifyGroupsChanged();

    if (params->verbose_output) {
        std::cout << "Outlier detection completed" << std::endl;
    }

    return line_data;
}

// LineOutlierDetectionOperation implementation

std::string LineOutlierDetectionOperation::getName() const {
    return "Line Outlier Detection";
}

std::type_index LineOutlierDetectionOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<LineData>);
}

bool LineOutlierDetectionOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineOutlierDetectionOperation::getDefaultParameters() const {
    // Return parameters with null group manager - must be set before execution
    return std::make_unique<LineOutlierDetectionParameters>(nullptr);
}

DataTypeVariant LineOutlierDetectionOperation::execute(DataTypeVariant const & dataVariant,
                                                       TransformParametersBase const * transformParameters) {
    // Delegate to overload with no-op progress callback
    return execute(dataVariant, transformParameters, [](int) { /* No progress reporting */ });
}

DataTypeVariant LineOutlierDetectionOperation::execute(DataTypeVariant const & dataVariant,
                                                       TransformParametersBase const * transformParameters,
                                                       ProgressCallback progressCallback) {
    if (!std::holds_alternative<std::shared_ptr<LineData>>(dataVariant)) {
        std::cerr << "Error: incorrect data type for Line Outlier Detection operation" << std::endl;
        return dataVariant;
    }

    auto const * params = dynamic_cast<LineOutlierDetectionParameters const *>(transformParameters);
    if (!params) {
        std::cerr << "Error: incorrect parameter type for Line Outlier Detection operation" << std::endl;
        return dataVariant;
    }

    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    auto result = ::lineOutlierDetection(line_data, params, progressCallback);

    return DataTypeVariant(result);
}
