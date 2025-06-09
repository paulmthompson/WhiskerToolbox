#ifndef MVP_ANALOGTIMESERIES_HPP
#define MVP_ANALOGTIMESERIES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

#include "DisplayOptions/TimeSeriesDisplayOptions.hpp"

struct AnalogTimeSeriesDisplayOptions;
struct PlottingManager;


/**
 * @brief Scaling configuration for analog time series data
 * 
 * Defines the three categories of scaling: intrinsic (data-based), 
 * user-specified, and global scaling factors.
 */
struct AnalogScalingConfig {
    // Intrinsic scaling based on data characteristics
    float intrinsic_scale{1.0f}; ///< Normalization based on data properties (e.g., 3*std_dev)
    float intrinsic_offset{0.0f};///< Data-based vertical offset

    // User-specified scaling controls
    float user_scale_factor{1.0f};   ///< User-controlled amplitude scaling
    float user_vertical_offset{0.0f};///< User-controlled vertical positioning

    // Global scaling applied to all series
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor
};

/**
 * @brief Display options for new analog time series visualization system
 * 
 * Comprehensive configuration for analog series display including scaling,
 * positioning, and visual properties.
 */
struct NewAnalogTimeSeriesDisplayOptions {
    // Visual properties
    std::string hex_color{"#007bff"};
    float alpha{1.0f};
    bool is_visible{true};
    int line_thickness{1};

    // Scaling configuration
    AnalogScalingConfig scaling;

    // Legacy compatibility members for backward compatibility with OpenGLWidget
    float scale_factor{1.0f};                                        ///< Legacy scale factor (computed from std dev)
    float user_scale_factor{1.0f};                                   ///< Legacy user scale factor
    float y_offset{0.0f};                                            ///< Legacy Y offset for positioning
    AnalogGapHandling gap_handling{AnalogGapHandling::AlwaysConnect};///< Gap handling mode
    bool enable_gap_detection{false};                                ///< Enable automatic gap detection
    float gap_threshold{5.0f};                                       ///< Threshold for gap detection

    // Positioning allocated by PlottingManager
    float allocated_y_center{0.0f};///< Y-coordinate center allocated by plotting manager
    float allocated_height{1.0f};  ///< Height allocated by plotting manager

    // Data range information (for optimization)
    mutable float cached_std_dev{0.0f};
    mutable bool std_dev_cache_valid{false};
    mutable float cached_mean{0.0f};
    mutable bool mean_cache_valid{false};
};

/**
 * @brief Create new Model matrix for analog series positioning and scaling
 *
 * New implementation that handles the three-tier scaling system:
 * intrinsic (data-based), user-specified, and global scaling.
 * Data is centered around its mean value for proper visual centering.
 *
 * @param display_options New display configuration for the analog series
 * @param std_dev Standard deviation of the data for intrinsic scaling
 * @param data_mean Mean value of the data for intrinsic centering
 * @param plotting_manager Reference to plotting manager for coordinate allocation
 * @return Model transformation matrix
 */
glm::mat4 new_getAnalogModelMat(NewAnalogTimeSeriesDisplayOptions const & display_options,
                                float std_dev,
                                float data_mean,
                                PlottingManager const & plotting_manager);

/**
 * @brief Create new View matrix for analog series global transformations
 *
 * New implementation for view-level transformations applied to all analog series.
 * Handles global panning and view-specific transformations.
 *
 * @param plotting_manager Reference to plotting manager for global transformations
 * @return View transformation matrix
 */
glm::mat4 new_getAnalogViewMat(PlottingManager const & plotting_manager);

/**
 * @brief Create new Projection matrix for analog series coordinate mapping
 *
 * New implementation that maps data coordinates to normalized device coordinates.
 * Takes data index range instead of time range for testing purposes.
 *
 * @param start_data_index Start index of data range to display (e.g., 1)
 * @param end_data_index End index of data range to display (e.g., 10000)
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @param plotting_manager Reference to plotting manager for viewport configuration
 * @return Projection transformation matrix
 */
glm::mat4 new_getAnalogProjectionMat(int start_data_index,
                                     int end_data_index,
                                     float y_min,
                                     float y_max,
                                     PlottingManager const & plotting_manager);


/**
 * @brief Calculate the mean of a data vector
 * 
 * Helper function to calculate the arithmetic mean of data values.
 * Used for intrinsic data centering in the scaling system.
 * 
 * @param data Vector of data values
 * @return Arithmetic mean of the data
 */
float calculateDataMean(std::vector<float> const & data);

/**
 * @brief Set intrinsic properties for analog display options
 * 
 * Calculates and caches the mean and standard deviation for a dataset,
 * storing them in the display options for use during MVP matrix generation.
 * 
 * @param data Vector of data values
 * @param display_options Display options to update with intrinsic properties
 */
void setAnalogIntrinsicProperties(std::vector<float> const & data,
                                  NewAnalogTimeSeriesDisplayOptions & display_options);

#endif// MVP_ANALOGTIMESERIES_HPP
