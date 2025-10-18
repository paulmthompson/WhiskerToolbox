#ifndef LINE_GROUP_TO_INTERVALS_HPP
#define LINE_GROUP_TO_INTERVALS_HPP

#include "transforms/data_transforms.hpp"
#include "Entity/EntityTypes.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <memory>
#include <string>
#include <typeindex>

class LineData;
class DigitalIntervalSeries;

/**
 * @brief Parameters for converting line group presence to digital intervals
 * 
 * This transformation analyzes LineData to determine which TimeFrameIndex values
 * contain lines that are members of a specified group. It then creates a 
 * DigitalIntervalSeries representing continuous sequences of frames where the
 * group is either present or absent.
 * 
 * Use cases:
 * - Track when specific whiskers (groups) are detected across frames
 * - Identify gaps in tracking for quality control
 * - Create binary signals for correlation analysis with other time series data
 */
struct LineGroupToIntervalsParameters : public TransformParametersBase {
    /**
     * @brief Pointer to the EntityGroupManager for group lookups
     * 
     * This is required to query which entities belong to the target group.
     * Must be set before execution.
     */
    EntityGroupManager* group_manager = nullptr;

    /**
     * @brief The GroupId to track across frames
     * 
     * The transformation will check each TimeFrameIndex for lines that
     * are members of this group.
     */
    GroupId target_group_id = 0;

    /**
     * @brief If true, create intervals where group is PRESENT; if false, where ABSENT
     * 
     * - true (default): Output intervals represent frames containing group members
     * - false: Output intervals represent frames WITHOUT group members (gaps)
     */
    bool track_presence = true;

    /**
     * @brief Minimum number of consecutive frames to form an interval
     * 
     * Intervals shorter than this will be filtered out. Useful for removing
     * noise or brief detection artifacts.
     * Default: 1 (no filtering)
     */
    int min_interval_length = 1;

    /**
     * @brief If true, merge intervals separated by gaps smaller than this value
     * 
     * Set to > 1 to bridge small gaps in detection. For example, if set to 3,
     * intervals separated by 1-2 frames will be merged into a single interval.
     * Default: 1 (no merging)
     */
    int merge_gap_threshold = 1;
};

/**
 * @brief Convert line group presence/absence to digital interval series
 * 
 * This function analyzes each TimeFrameIndex in the LineData to determine if
 * any lines at that time are members of the specified group. It then constructs
 * continuous intervals representing the presence or absence of the group.
 * 
 * Algorithm:
 * 1. For each TimeFrameIndex with line data:
 *    - Get all EntityIds at that time
 *    - Check if any entity is a member of target_group_id
 *    - Mark frame as "active" or "inactive" based on track_presence parameter
 * 2. Identify continuous runs of "active" frames
 * 3. Convert runs to Interval objects
 * 4. Apply filtering (min_interval_length) and merging (merge_gap_threshold)
 * 5. Create DigitalIntervalSeries from resulting intervals
 * 
 * @param line_data The LineData to analyze
 * @param params Parameters including group_manager, target_group_id, and options
 * @return A new DigitalIntervalSeries representing the group presence/absence
 */
std::shared_ptr<DigitalIntervalSeries> lineGroupToIntervals(
    std::shared_ptr<LineData> const & line_data,
    LineGroupToIntervalsParameters const * params);

/**
 * @brief Overload with progress callback support
 */
std::shared_ptr<DigitalIntervalSeries> lineGroupToIntervals(
    std::shared_ptr<LineData> const & line_data,
    LineGroupToIntervalsParameters const * params,
    ProgressCallback progressCallback);

/**
 * @brief Transform operation for converting line group membership to intervals
 */
class LineGroupToIntervalsOperation final : public TransformOperation {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the operation.
     * @return A unique_ptr to the default parameters.
     * @note The EntityGroupManager and target_group_id must be set before execution.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

#endif // LINE_GROUP_TO_INTERVALS_HPP
