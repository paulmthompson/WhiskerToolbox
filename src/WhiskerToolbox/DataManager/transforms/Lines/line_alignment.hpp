#ifndef LINE_ALIGNMENT_HPP
#define LINE_ALIGNMENT_HPP

#include "transforms/data_transforms.hpp"

#include "CoreGeometry/points.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/ImageSize.hpp"

#include <memory>       // std::shared_ptr
#include <string>       // std::string
#include <typeindex>    // std::type_index
#include <vector>       // std::vector

class LineData;
class MediaData;

/**
 * @brief Enum for FWHM calculation approach
 */
enum class FWHMApproach {
    PEAK_WIDTH_HALF_MAX,    // Width at 1/2 maximum height
    GAUSSIAN_FIT,           // Gaussian curve fitting
    THRESHOLD_BASED         // Threshold-based peak detection
};

/**
 * @brief Calculate perpendicular direction at a line vertex
 * 
 * @param line The line containing the vertex
 * @param vertex_index The index of the vertex
 * @return The perpendicular direction as a normalized vector
 */
Point2D<float> calculate_perpendicular_direction(Line2D const & line, size_t vertex_index);

/**
 * @brief Calculate FWHM displacement for a single vertex
 * 
 * @param vertex The vertex position
 * @param perpendicular_dir The perpendicular direction
 * @param width The width of the analysis strip
 * @param image_data The image data as uint8_t vector
 * @param image_size The image dimensions
 * @param approach The FWHM calculation approach
 * @return The displacement along the perpendicular direction
 */
float calculate_fwhm_displacement(Point2D<float> const & vertex,
                                 Point2D<float> const & perpendicular_dir,
                                 int width,
                                 std::vector<uint8_t> const & image_data,
                                 ImageSize const & image_size,
                                 FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);

/**
 * @brief Align a line to bright linear objects in media data
 * 
 * @param line_data The input line data
 * @param media_data The media data containing the images
 * @param width The width of the analysis strip perpendicular to the line
 * @param use_processed_data Whether to use processed or raw media data
 * @param approach The FWHM calculation approach
 * @return A new LineData with aligned vertices
 */
std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width = 20,
                                         bool use_processed_data = true,
                                         FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX);

/**
 * @brief Align a line to bright linear objects in media data with progress callback
 * 
 * @param line_data The input line data
 * @param media_data The media data containing the images
 * @param width The width of the analysis strip perpendicular to the line
 * @param use_processed_data Whether to use processed or raw media data
 * @param approach The FWHM calculation approach
 * @param progressCallback Progress callback function
 * @return A new LineData with aligned vertices
 */
std::shared_ptr<LineData> line_alignment(LineData const * line_data,
                                         MediaData * media_data,
                                         int width,
                                         bool use_processed_data,
                                         FWHMApproach approach,
                                         ProgressCallback progressCallback);

///////////////////////////////////////////////////////////////////////////////

struct LineAlignmentParameters : public TransformParametersBase {
    std::shared_ptr<MediaData> media_data;  // Pointer to the MediaData
    int width = 20;                         // Width of analysis strip
    bool use_processed_data = true;         // Whether to use processed or raw data
    FWHMApproach approach = FWHMApproach::PEAK_WIDTH_HALF_MAX; // FWHM calculation approach
};

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

#endif//LINE_ALIGNMENT_HPP 