#ifndef LINE_PROXIMITY_GROUPING_HPP
#define LINE_PROXIMITY_GROUPING_HPP

#include "transforms/grouping_transforms.hpp"
#include "CoreGeometry/lines.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;

/**
 * @brief Parameters for the line proximity grouping operation
 * 
 * This operation groups lines based on their proximity to existing groups.
 * Lines within a threshold distance of the nearest point in an existing group
 * will be assigned to that group.
 */
struct LineProximityGroupingParameters : public GroupingTransformParametersBase {
    explicit LineProximityGroupingParameters(EntityGroupManager* group_manager)
        : GroupingTransformParametersBase(group_manager) {}

    float distance_threshold = 50.0f;  // Maximum distance to consider for grouping
    float position_along_line = 0.5f;  // Position along line to use for distance calculation (0.0-1.0)
    bool create_new_group_for_outliers = true;  // Create a new group for lines that don't fit existing groups
    std::string new_group_name = "Ungrouped Lines";  // Name for the new group if created
};

/**
 * @brief Calculate the distance between two points on lines at specified positions
 * 
 * @param line1 First line
 * @param line2 Second line
 * @param position Position along each line (0.0-1.0)
 * @return Distance between the points
 */
float calculateLineDistance(Line2D const& line1, Line2D const& line2, float position);

/**
 * @brief Find the closest group to a line based on minimum distance to any line in the group
 * 
 * @param target_line The line to find a group for
 * @param line_data The LineData containing all lines
 * @param group_manager The EntityGroupManager containing existing groups
 * @param position Position along lines to use for distance calculation
 * @return Pair of {GroupId, distance} - GroupId of the closest group (0 if no groups exist) and the distance
 */
std::pair<GroupId, float> findClosestGroup(Line2D const& target_line, 
                                           LineData const* line_data,
                                           EntityGroupManager* group_manager, 
                                           float position);

/**
 * @brief Group lines by proximity to existing groups
 * 
 * This function examines all ungrouped lines and assigns them to the nearest
 * existing group if they are within the distance threshold. If no suitable
 * group is found and create_new_group_for_outliers is true, a new group
 * will be created for outliers.
 * 
 * @param line_data The LineData to process
 * @param params Parameters including distance threshold and grouping options
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineProximityGrouping(std::shared_ptr<LineData> line_data,
                                               LineProximityGroupingParameters const* params);

std::shared_ptr<LineData> lineProximityGrouping(std::shared_ptr<LineData> line_data,
                                               LineProximityGroupingParameters const* params,
                                               ProgressCallback progressCallback);

/**
 * @brief Transform operation for grouping lines by proximity
 */
class LineProximityGroupingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;
    
    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;
    
    [[nodiscard]] bool canApply(DataTypeVariant const& dataVariant) const override;
    
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;
    
    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                           TransformParametersBase const* transformParameters,
                           ProgressCallback progressCallback) override;
                           
    DataTypeVariant execute(DataTypeVariant const& dataVariant,
                           TransformParametersBase const* transformParameters) override;
};

#endif//LINE_PROXIMITY_GROUPING_HPP