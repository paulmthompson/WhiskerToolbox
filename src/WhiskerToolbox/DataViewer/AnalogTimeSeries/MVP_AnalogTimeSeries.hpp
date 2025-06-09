#ifndef MVP_ANALOGTIMESERIES_HPP
#define MVP_ANALOGTIMESERIES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

struct AnalogTimeSeriesDisplayOptions;

/**
 * @brief Create Model matrix for analog series positioning and scaling
 *
 * Handles series-specific transformations for analog series including
 * VerticalSpaceManager positioning with allocated space or legacy index-based
 * positioning. Applies amplitude scaling based on data characteristics and user controls.
 *
 * @param display_options Display configuration for the analog series
 * @param key Series key for debugging output
 * @param stdDev Standard deviation of the data for amplitude scaling
 * @param series_index Index of this series for legacy positioning
 * @param ySpacing Vertical spacing between series for legacy positioning
 * @param center_coord Centering coordinate for legacy positioning
 * @param global_zoom Global zoom factor applied to all series
 * @return Model transformation matrix
 */
glm::mat4 getAnalogModelMat(AnalogTimeSeriesDisplayOptions const * display_options,
                            std::string const & key,
                            float stdDev,
                            int series_index,
                            float ySpacing,
                            float center_coord,
                            float global_zoom);

/**
 * @brief Create View matrix for analog series global transformations
 *
 * Handles view-level transformations applied to all analog series.
 * Currently returns identity matrix as pan offset is handled in projection.
 *
 * @return View transformation matrix
 */
glm::mat4 getAnalogViewMat();

/**
 * @brief Create Projection matrix for analog series coordinate mapping
 *
 * Maps world coordinates to screen coordinates for analog series,
 * including dynamic viewport adjustments for panning and zooming.
 *
 * @param start_time Start time of visible range
 * @param end_time End time of visible range
 * @param yMin Minimum Y coordinate of viewport
 * @param yMax Maximum Y coordinate of viewport
 * @param verticalPanOffset Vertical pan offset for dynamic viewport
 * @return Projection transformation matrix
 */
glm::mat4 getAnalogProjectionMat(float start_time,
                                 float end_time,
                                 float yMin,
                                 float yMax,
                                 float verticalPanOffset);

// NEW INFRASTRUCTURE - Building from scratch

/**
 * @brief Scaling configuration for analog time series data
 * 
 * Defines the three categories of scaling: intrinsic (data-based), 
 * user-specified, and global scaling factors.
 */
struct AnalogScalingConfig {
    // Intrinsic scaling based on data characteristics
    float intrinsic_scale{1.0f};        ///< Normalization based on data properties (e.g., 3*std_dev)
    float intrinsic_offset{0.0f};       ///< Data-based vertical offset
    
    // User-specified scaling controls
    float user_scale_factor{1.0f};      ///< User-controlled amplitude scaling
    float user_vertical_offset{0.0f};   ///< User-controlled vertical positioning
    
    // Global scaling applied to all series
    float global_zoom{1.0f};            ///< Global zoom factor
    float global_vertical_scale{1.0f};  ///< Global vertical scale factor
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
    
    // Positioning allocated by PlottingManager
    float allocated_y_center{0.0f};     ///< Y-coordinate center allocated by plotting manager
    float allocated_height{1.0f};       ///< Height allocated by plotting manager
    
    // Data range information (for optimization)
    mutable float cached_std_dev{0.0f};
    mutable bool std_dev_cache_valid{false};
    mutable float cached_mean{0.0f};
    mutable bool mean_cache_valid{false};
};

// Forward declaration for PlottingManager
struct PlottingManager;

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
 * @brief Plotting manager for coordinating multiple data series visualization
 * 
 * Manages the display of multiple time series with different data types,
 * handles coordinate allocation, global scaling, and viewport management.
 * Intended to eventually replace VerticalSpaceManager.
 */
struct PlottingManager {
    // Global scaling and positioning
    float global_zoom{1.0f};              ///< Global zoom factor applied to all series
    float global_vertical_scale{1.0f};    ///< Global vertical scaling
    float vertical_pan_offset{0.0f};      ///< Global vertical pan offset
    
    // Viewport configuration
    float viewport_y_min{-1.0f};          ///< Minimum Y coordinate of viewport in NDC
    float viewport_y_max{1.0f};           ///< Maximum Y coordinate of viewport in NDC
    
    // Data coordinate system
    int total_data_points{0};              ///< Total number of data points in the dataset
    int visible_start_index{0};           ///< Start index of visible data range
    int visible_end_index{0};             ///< End index of visible data range
    
    // Series management
    int total_analog_series{0};           ///< Number of analog series being displayed
    int total_digital_series{0};          ///< Number of digital series being displayed
    
    /**
     * @brief Calculate Y-coordinate allocation for an analog series
     * 
     * Determines the center Y coordinate and allocated height for a specific
     * analog series based on the total number of series and their arrangement.
     * 
     * @param series_index Index of the analog series (0-based)
     * @param allocated_center Output parameter for center Y coordinate
     * @param allocated_height Output parameter for allocated height
     */
    void calculateAnalogSeriesAllocation(int series_index, 
                                       float & allocated_center, 
                                       float & allocated_height) const;
    
    /**
     * @brief Set the visible data range for projection calculations
     * 
     * @param start_index Start index of visible data
     * @param end_index End index of visible data
     */
    void setVisibleDataRange(int start_index, int end_index);
    
    /**
     * @brief Add an analog series to the plotting manager
     * 
     * Registers a new analog series and updates internal bookkeeping
     * for coordinate allocation.
     * 
     * @return Series index for the newly added series
     */
    int addAnalogSeries();
};

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
