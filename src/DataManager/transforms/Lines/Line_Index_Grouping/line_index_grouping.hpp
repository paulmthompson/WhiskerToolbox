#ifndef LINE_INDEX_GROUPING_HPP
#define LINE_INDEX_GROUPING_HPP

#include "transforms/grouping_transforms.hpp"
#include "CoreGeometry/lines.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;

/**
 * @brief Parameters for the line index grouping operation
 * 
 * This operation creates groups based on the position of lines within the vector
 * at each timestamp. It finds the maximum number of lines at any timestamp and
 * creates that many groups, then assigns all lines at index 0 to group 0,
 * index 1 to group 1, etc.
 * 
 * This is useful for organizing whiskers or other tracked lines when detection
 * order provides a consistent identity across frames.
 */
struct LineIndexGroupingParameters : public GroupingTransformParametersBase {
    using GroupingTransformParametersBase::GroupingTransformParametersBase;

    // Prefix for group names (e.g., "Whisker" -> "Whisker 0", "Whisker 1", ...)
    std::string group_name_prefix = "Line";

    // Template for group descriptions (use {} as placeholder for index number)
    std::string group_description_template = "Group {} - lines at vector index {}";

    // If true, clear existing groups before creating new ones
    bool clear_existing_groups = false;
};

/**
 * @brief Group lines by their vector index at each timestamp
 * 
 * This function analyzes all timestamps in the LineData to find the maximum
 * number of lines at any single timestamp. It then creates that many groups
 * and assigns lines based on their position in the vector at each timestamp.
 * 
 * For example, if timestamps have [3, 5, 4, 2] lines respectively, it will
 * create 5 groups (0-4). Lines at index 0 across all timestamps go to group 0,
 * lines at index 1 go to group 1, etc.
 * 
 * @param line_data The LineData to process
 * @param params Parameters including group naming options
 * @return The same LineData shared_ptr (operation is performed in-place on groups)
 */
std::shared_ptr<LineData> lineIndexGrouping(std::shared_ptr<LineData> line_data,
                                            LineIndexGroupingParameters const * params);

std::shared_ptr<LineData> lineIndexGrouping(std::shared_ptr<LineData> line_data,
                                            LineIndexGroupingParameters const * params,
                                            ProgressCallback const & progressCallback);

/**
 * @brief Transform operation for grouping lines by vector index
 */
class LineIndexGroupingOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the index grouping operation.
     * @return A unique_ptr to the default parameters with null group manager.
     * @note The EntityGroupManager must be set via setGroupManager() before execution.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif// LINE_INDEX_GROUPING_HPP
