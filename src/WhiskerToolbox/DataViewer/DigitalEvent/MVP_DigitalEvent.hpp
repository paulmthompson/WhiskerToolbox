#ifndef MVP_DIGITALEVENT_HPP
#define MVP_DIGITALEVENT_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>

struct DigitalEventSeriesDisplayOptions;
struct PlottingManager;

/**
 * @brief Create Model matrix for digital event series positioning and scaling (legacy)
 *
 * Handles series-specific transformations for digital event series including
 * stacked positioning with VerticalSpaceManager or legacy index-based positioning.
 *
 * @param display_options Display configuration for the event series
 * @param visible_series_index Index of this series among visible series
 * @param center_coord Centering coordinate for legacy positioning
 * @return Model transformation matrix
 */
glm::mat4 getEventModelMat(DigitalEventSeriesDisplayOptions const * display_options,
                           int visible_series_index,
                           int center_coord);

/**
 * @brief Create View matrix for digital event series global transformations (legacy)
 *
 * Handles view-level transformations applied to all digital event series.
 * Currently returns identity matrix as pan offset is handled in projection.
 *
 * @return View transformation matrix
 */
glm::mat4 getEventViewMat();

/**
 * @brief Create Projection matrix for digital event series coordinate mapping (legacy)
 *
 * Maps world coordinates to screen coordinates for digital event series,
 * including dynamic viewport adjustments for panning and zooming.
 *
 * @param yMin Minimum Y coordinate of viewport
 * @param yMax Maximum Y coordinate of viewport
 * @param verticalPanOffset Vertical pan offset for dynamic viewport
 * @param start_time Start time of visible range
 * @param end_time End time of visible range
 * @return Projection transformation matrix
 */
glm::mat4 getEventProjectionMat(float yMin,
                                float yMax,
                                float verticalPanOffset,
                                int64_t start_time,
                                int64_t end_time);

// NEW INFRASTRUCTURE - Building from scratch

/**
 * @brief Plotting mode for digital event series
 */
enum class EventPlottingMode {
    FullCanvas,///< Events extend from top to bottom of entire plot (like digital intervals)
    Stacked    ///< Events are allocated a portion of canvas and extend within that space (like analog series)
};

/**
 * @brief Event data representing a single time point
 */
struct EventData {
    float time{0.0f};///< Time of the event occurrence

    /**
     * @brief Constructor for EventData
     * @param event_time Time of the event
     */
    explicit EventData(float event_time)
        : time(event_time) {}

    /**
     * @brief Default constructor
     */
    EventData() = default;

    /**
     * @brief Check if event time is valid (non-negative)
     * @return true if event time is valid
     */
    [[nodiscard]] bool isValid() const {
        return time >= 0.0f;
    }
};

/**
 * @brief Display options for new digital event series visualization system
 * 
 * Configuration for digital event series display including visual properties,
 * positioning, plotting mode, and rendering options. Events are rendered as 
 * vertical lines extending either across full canvas or within allocated space.
 */
struct NewDigitalEventSeriesDisplayOptions {
    // Visual properties
    std::string hex_color{"#ff9500"};///< Color of the events (default: orange)
    float alpha{0.8f};               ///< Alpha transparency for events (default: 80%)
    bool is_visible{true};           ///< Whether events are visible
    int line_thickness{2};           ///< Thickness of event lines

    // Plotting mode configuration
    EventPlottingMode plotting_mode{EventPlottingMode::FullCanvas};///< How events should be plotted

    // Global scaling applied by PlottingManager
    float global_zoom{1.0f};          ///< Global zoom factor
    float global_vertical_scale{1.0f};///< Global vertical scale factor

    // Positioning allocated by PlottingManager (used in Stacked mode)
    float allocated_y_center{0.0f};///< Y-coordinate center allocated by plotting manager
    float allocated_height{2.0f};  ///< Height allocated by plotting manager

    // Rendering options
    float margin_factor{0.95f};///< Margin factor for event height (0.95 = 95% of allocated space)
};

/**
 * @brief Create new Model matrix for digital event series positioning and scaling
 *
 * New implementation for digital event series that handles both plotting modes:
 * - FullCanvas: Events extend from top to bottom of entire plot (like intervals)
 * - Stacked: Events are positioned within allocated space (like analog series)
 *
 * @param display_options New display configuration for the event series
 * @param plotting_manager Reference to plotting manager for coordinate allocation
 * @return Model transformation matrix
 */
glm::mat4 new_getEventModelMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                               PlottingManager const & plotting_manager);

/**
 * @brief Create new View matrix for digital event series global transformations
 *
 * New implementation for view-level transformations applied to all digital event series.
 * Behavior depends on plotting mode:
 * - FullCanvas: No panning (events stay viewport-pinned like intervals)
 * - Stacked: Applies panning (events move with content like analog series)
 *
 * @param display_options Display configuration for mode-dependent behavior
 * @param plotting_manager Reference to plotting manager for global transformations
 * @return View transformation matrix
 */
glm::mat4 new_getEventViewMat(NewDigitalEventSeriesDisplayOptions const & display_options,
                              PlottingManager const & plotting_manager);

/**
 * @brief Create new Projection matrix for digital event series coordinate mapping
 *
 * New implementation that maps data coordinates to normalized device coordinates.
 * Takes data index range instead of time range for testing purposes.
 * Behavior is consistent across both plotting modes.
 *
 * @param start_data_index Start index of data range to display (e.g., 1)
 * @param end_data_index End index of data range to display (e.g., 10000)
 * @param y_min Minimum Y coordinate in data space
 * @param y_max Maximum Y coordinate in data space
 * @param plotting_manager Reference to plotting manager for viewport configuration
 * @return Projection transformation matrix
 */
glm::mat4 new_getEventProjectionMat(int start_data_index,
                                    int end_data_index,
                                    float y_min,
                                    float y_max,
                                    PlottingManager const & plotting_manager);

/**
 * @brief Generate test digital event data
 * 
 * Creates a vector of events with times between 0 and max_time.
 * Events are ordered by time for optimal visualization.
 * 
 * @param num_events Number of events to generate
 * @param max_time Maximum time value for event generation
 * @param seed Random seed for reproducibility
 * @return Vector of generated event data
 */
std::vector<EventData> generateTestEventData(size_t num_events,
                                             float max_time = 10000.0f,
                                             unsigned int seed = 42);

/**
 * @brief Set intrinsic properties for digital event display options
 * 
 * Analyzes event data and configures display options with appropriate
 * settings for optimal visualization.
 * 
 * @param events Vector of event data to analyze
 * @param display_options Display options to configure
 */
void setEventIntrinsicProperties(std::vector<EventData> const & events,
                                 NewDigitalEventSeriesDisplayOptions & display_options);

#endif// MVP_DIGITALEVENT_HPP
