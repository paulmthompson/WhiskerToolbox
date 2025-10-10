#ifndef LINE_ALIGNMENT_HPP
#define LINE_ALIGNMENT_HPP


#include "transforms/data_transforms.hpp"
#include "transforms/grouping_transforms.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/line_geometry.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/points.hpp"

#include <memory>   // std::shared_ptr
#include <string>   // std::string
#include <typeindex>// std::type_index
#include <vector>   // std::vector

class LineData;
class MediaData;

/**
 * @brief Enum for FWHM calculation approach
 */
enum class FWHMApproach {
    PEAK_WIDTH_HALF_MAX,// Width at 1/2 maximum height
    GAUSSIAN_FIT,       // Gaussian curve fitting
    THRESHOLD_BASED     // Threshold-based peak detection
};

/**
 * @brief Enum for output mode
 */
enum class LineAlignmentOutputMode {
    ALIGNED_VERTICES,   // Return aligned line vertices (normal mode)
    FWHM_PROFILE_EXTENTS// Return FWHM profile extents for debugging/analysis
};


template<typename T>
T get_pixel_value(Point2D<float> const & point,
                  std::vector<T> const & image_data,
                  ImageSize const & image_size);


struct LineAlignmentParameters : public GroupingTransformParametersBase {
    explicit LineAlignmentParameters(EntityGroupManager * group_manager = nullptr)
        : GroupingTransformParametersBase(group_manager) {}

    std::shared_ptr<MediaData> media_data;                                          // Pointer to the MediaData
    int width = 20;                                                                 // Width of analysis strip
    int perpendicular_range = 50;                                                   // Range of perpendicular analysis (pixels)
    bool use_processed_data = true;                                                 // Whether to use processed or raw data
    FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX;                      // FWHM calculation approach
    LineAlignmentOutputMode output_mode = LineAlignmentOutputMode::ALIGNED_VERTICES;// Output mode

    // === Grouping Parameters ===
    bool enable_grouping = true;                                    // Enable grouping of FWHM lines by vertex index
    std::string group_prefix = "FWHM_Vertex_";                      // Prefix for generated group names
    std::string group_description = "FWHM profile lines for vertex";// Description for generated groups
};


/**
 * @brief Calculate FWHM center point for a single vertex
 * 
 * @param vertex The vertex position
 * @param perpendicular_dir The perpendicular direction
 * @param width The width of the analysis strip
 * @param image_data The image data as uint8_t vector
 * @param image_size The image dimensions
 * @param approach The FWHM calculation approach
 * @return The center point of the bright feature along the perpendicular direction
 */
template<typename T>
Point2D<float> calculate_fwhm_center(Point2D<float> const & vertex,
                                     Point2D<float> const & perpendicular_dir,
                                     int width,
                                     int perpendicular_range,
                                     std::vector<T> const & image_data,
                                     ImageSize const & image_size,
                                     FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);

/**
 * @brief Calculate FWHM profile extents for a single vertex
 * 
 * @param vertex The vertex position
 * @param perpendicular_dir The perpendicular direction
 * @param width The width of the analysis strip
 * @param image_data The image data as uint8_t vector
 * @param image_size The image dimensions
 * @param approach The FWHM calculation approach
 * @return A line with 3 points: [left_extent, max_point, right_extent]
 */
template<typename T>
Line2D calculate_fwhm_profile_extents(Point2D<float> const & vertex,
                                      Point2D<float> const & perpendicular_dir,
                                      int width,
                                      int perpendicular_range,
                                      std::vector<T> const & image_data,
                                      ImageSize const & image_size,
                                      FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);

/**
 * @brief Align a line to bright linear objects in media data
 * 
 * @param line_data The input line data
 * @param media_data The media data containing the images
 * @param width The width of the analysis strip perpendicular to the line
 * @param perpendicular_range The range of perpendicular analysis
 * @param use_processed_data Whether to use processed or raw media data
 * @param approach The FWHM calculation approach
 * @param output_mode The output mode (aligned vertices or FWHM profile extents)
 * @param params Parameters including grouping settings
 * @return A new LineData with aligned vertices or FWHM profile extents
 */
std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width = 20,
                                         int perpendicular_range = 50,
                                         bool use_processed_data = true,
                                         FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX,
                                         LineAlignmentOutputMode output_mode = LineAlignmentOutputMode::ALIGNED_VERTICES,
                                         LineAlignmentParameters const * params = nullptr);

/**
 * @brief Align a line to bright linear objects in media data with progress callback
 * 
 * @param line_data The input line data
 * @param media_data The media data containing the images
 * @param width The width of the analysis strip perpendicular to the line
 * @param perpendicular_range The range of perpendicular analysis
 * @param use_processed_data Whether to use processed or raw media data
 * @param approach The FWHM calculation approach
 * @param output_mode The output mode (aligned vertices or FWHM profile extents)
 * @param params Parameters including grouping settings
 * @param progressCallback Progress callback function
 * @return A new LineData with aligned vertices or FWHM profile extents
 */
std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width,
                                         int perpendicular_range,
                                         bool use_processed_data,
                                         FWHMApproach approach,
                                         LineAlignmentOutputMode output_mode,
                                         LineAlignmentParameters const * params,
                                         ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////


class LineAlignmentOperation final : public TransformOperation {
public:
    /**
     * @brief Gets the user-friendly name of this operation.
     * @return The name as a string.
     */
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::type_index getTargetInputTypeIndex() const override;

    /**
     * @brief Checks if this operation can be applied to the given data variant.
     * @param dataVariant The variant holding a shared_ptr to the data object.
     * @return True if the variant holds a non-null LineData shared_ptr, false otherwise.
     */
    [[nodiscard]] bool canApply(DataTypeVariant const & dataVariant) const override;

    /**
     * @brief Gets the default parameters for the line alignment operation.
     * @return A unique_ptr to the default parameters.
     * @note Returns nullptr since we can't create a GroupingTransformParametersBase
     * without an EntityGroupManager pointer. The calling code will need to provide
     * the actual parameters with the group manager.
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getDefaultParameters() const override;

    /**
     * @brief Executes the line alignment calculation using data from the variant.
     * @param dataVariant The variant holding a non-null shared_ptr to the LineData object.
     * @param transformParameters Parameters for the calculation, including the media data pointer.
     * @return DataTypeVariant containing a std::shared_ptr<LineData> on success,
     * or an empty variant on failure.
     */
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters) override;

    // Overload with progress
    DataTypeVariant execute(DataTypeVariant const & dataVariant,
                            TransformParametersBase const * transformParameters,
                            ProgressCallback progressCallback) override;
};

extern template uint8_t get_pixel_value<uint8_t>(Point2D<float> const & point,
                                                 std::vector<uint8_t> const & image_data,
                                                 ImageSize const & image_size);

extern template float get_pixel_value<float>(Point2D<float> const & point,
                                             std::vector<float> const & image_data,
                                             ImageSize const & image_size);


extern template Point2D<float> calculate_fwhm_center<uint8_t>(Point2D<float> const & vertex,
                                                              Point2D<float> const & perpendicular_dir,
                                                              int width,
                                                              int perpendicular_range,
                                                              std::vector<uint8_t> const & image_data,
                                                              ImageSize const & image_size,
                                                              FWHMApproach approach);

extern template Point2D<float> calculate_fwhm_center<float>(Point2D<float> const & vertex,
                                                            Point2D<float> const & perpendicular_dir,
                                                            int width,
                                                            int perpendicular_range,
                                                            std::vector<float> const & image_data,
                                                            ImageSize const & image_size,
                                                            FWHMApproach approach);


extern template Line2D calculate_fwhm_profile_extents<uint8_t>(Point2D<float> const & vertex,
                                                               Point2D<float> const & perpendicular_dir,
                                                               int width,
                                                               int perpendicular_range,
                                                               std::vector<uint8_t> const & image_data,
                                                               ImageSize const & image_size,
                                                               FWHMApproach approach);
extern template Line2D calculate_fwhm_profile_extents<float>(Point2D<float> const & vertex,
                                                             Point2D<float> const & perpendicular_dir,
                                                             int width,
                                                             int perpendicular_range,
                                                             std::vector<float> const & image_data,
                                                             ImageSize const & image_size,
                                                             FWHMApproach approach);


#endif//LINE_ALIGNMENT_HPP
