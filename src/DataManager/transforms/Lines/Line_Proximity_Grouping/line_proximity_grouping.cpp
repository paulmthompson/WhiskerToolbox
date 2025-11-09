#include "line_proximity_grouping.hpp"

#include "Lines/Line_Data.hpp"
#include "Entity/EntityGroupManager.hpp"
#include "Entity/EntityTypes.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "transforms/utils/variant_type_check.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

float calculateLineDistance(Line2D const& line1, Line2D const& line2, float position) {
    // Get points at the specified position along each line
    auto point1_opt = point_at_fractional_position(line1, position, true);
    auto point2_opt = point_at_fractional_position(line2, position, true);
    
    if (!point1_opt.has_value() || !point2_opt.has_value()) {
        return std::numeric_limits<float>::max();
    }
    
    auto point1 = point1_opt.value();
    auto point2 = point2_opt.value();
    
    // Calculate Euclidean distance
    float dx = point1.x - point2.x;
    float dy = point1.y - point2.y;
    return std::sqrt(dx * dx + dy * dy);
}

std::pair<GroupId, float> findClosestGroup(Line2D const& target_line, 
                                           LineData const* line_data,
                                           EntityGroupManager* group_manager, 
                                           float position) {
    auto all_group_ids = group_manager->getAllGroupIds();
    if (all_group_ids.empty()) {
        return {0, std::numeric_limits<float>::max()};
    }
    
    GroupId closest_group_id = 0;
    float min_distance = std::numeric_limits<float>::max();
    
    for (auto group_id : all_group_ids) {
        auto entities_in_group = group_manager->getEntitiesInGroup(group_id);
        
        for (auto entity_id : entities_in_group) {
            // Get the line corresponding to this entity
            auto line_opt = line_data->getDataByEntityId(entity_id);
            if (line_opt.has_value()) {
                float distance = calculateLineDistance(target_line, line_opt.value(), position);
                if (distance < min_distance) {
                    min_distance = distance;
                    closest_group_id = group_id;
                }
            }
        }
    }
    
    return {closest_group_id, min_distance};
}

std::shared_ptr<LineData> lineProximityGrouping(std::shared_ptr<LineData> line_data,
                                               LineProximityGroupingParameters const* params) {
    return lineProximityGrouping(std::move(line_data), params, [](int) {});
}

std::shared_ptr<LineData> lineProximityGrouping(std::shared_ptr<LineData> line_data,
                                               LineProximityGroupingParameters const* params,
                                               ProgressCallback progressCallback) {
    if (!line_data || !params || !params->getGroupManager()) {
        return line_data;  // Return the same shared_ptr
    }
    
    auto group_manager = params->getGroupManager();
    
    
    // Find entities that are not in any group
    std::unordered_set<EntityId> ungrouped_entities;
    for (auto const & [time, entries]: line_data->getAllEntries()) {
        for (auto const & entry: entries) {
            auto entity_id = entry.entity_id;
            auto groups_containing_entity = group_manager->getGroupsContainingEntity(entity_id);
            if (groups_containing_entity.empty()) {
                ungrouped_entities.insert(entity_id);
            }
        }
    }
    
    if (ungrouped_entities.empty()) {
        // No ungrouped entities, nothing to do
        progressCallback(100);
        return line_data;  // Return the same shared_ptr
    }
    
    // Process each ungrouped entity
    std::vector<EntityId> outliers;
    size_t processed = 0;
    size_t total = ungrouped_entities.size();
    
    for (auto entity_id : ungrouped_entities) {
        progressCallback(static_cast<int>((processed * 100) / total));
        
        auto line_opt = line_data->getDataByEntityId(entity_id);
        if (!line_opt.has_value()) {
            ++processed;
            continue;
        }
        
        auto [closest_group_id, min_distance] = findClosestGroup(
            line_opt.value(), line_data.get(), group_manager, params->position_along_line);
        
        if (closest_group_id != 0 && min_distance <= params->distance_threshold) {
            // Add to the closest group
            group_manager->addEntityToGroup(closest_group_id, entity_id);
        } else {
            // This entity doesn't fit any existing group
            outliers.push_back(entity_id);
        }
        
        ++processed;
    }
    
    // Handle outliers if requested
    if (!outliers.empty() && params->create_new_group_for_outliers) {
        auto new_group_id = group_manager->createGroup(params->new_group_name, 
            "Automatically created group for lines that don't fit existing groups");
        group_manager->addEntitiesToGroup(new_group_id, outliers);
    }
    
    progressCallback(100);
    
    // Return the same shared_ptr (operation was performed in-place on groups)
    return line_data;
}

// LineProximityGroupingOperation implementation

std::string LineProximityGroupingOperation::getName() const {
    return "Group Lines by Proximity";
}

std::type_index LineProximityGroupingOperation::getTargetInputTypeIndex() const {
    return std::type_index(typeid(std::shared_ptr<LineData>));
}

bool LineProximityGroupingOperation::canApply(DataTypeVariant const& dataVariant) const {
    return canApplyToType<LineData>(dataVariant);
}

std::unique_ptr<TransformParametersBase> LineProximityGroupingOperation::getDefaultParameters() const {
    // Note: This returns nullptr since we can't create a GroupingTransformParametersBase
    // without an EntityGroupManager pointer. The calling code will need to provide
    // the actual parameters with the group manager.
    return nullptr;
}

DataTypeVariant LineProximityGroupingOperation::execute(DataTypeVariant const& dataVariant,
                                                       TransformParametersBase const* transformParameters) {
    return execute(dataVariant, transformParameters, [](int) {});
}

DataTypeVariant LineProximityGroupingOperation::execute(DataTypeVariant const& dataVariant,
                                                       TransformParametersBase const* transformParameters,
                                                       ProgressCallback progressCallback) {
    if (!canApply(dataVariant)) {
        return DataTypeVariant{};
    }
    
    auto line_data = std::get<std::shared_ptr<LineData>>(dataVariant);
    auto params = dynamic_cast<LineProximityGroupingParameters const*>(transformParameters);
    
    if (!params) {
        return DataTypeVariant{};
    }
    
    auto result = lineProximityGrouping(line_data, params, progressCallback);
    return DataTypeVariant{result};
}