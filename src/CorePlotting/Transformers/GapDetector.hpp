#ifndef COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP
#define COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP

#include "../SceneGraph/RenderablePrimitives.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

namespace CorePlotting {

/**
 * @brief Detects gaps in analog time series and produces segmented poly-line batches
 * 
 * This transformer analyzes an AnalogTimeSeries for discontinuities (gaps) based on
 * either:
 * 1. Time threshold: If time between samples exceeds threshold, insert gap
 * 2. Value threshold: If value change exceeds threshold, insert gap
 * 
 * The output is a RenderablePolyLineBatch with multiple line segments, where each
 * segment is a contiguous portion of the signal without gaps.
 * 
 * USAGE:
 * ```cpp
 * GapDetector detector;
 * detector.setTimeThreshold(1000);  // 1000 time units
 * RenderablePolyLineBatch batch = detector.transform(series, time_frame, entity_id);
 * ```
 */
class GapDetector {
public:
    /**
     * @brief Configuration for gap detection
     */
    struct Config {
        /// Maximum allowed time between samples (in time frame units)
        /// If exceeded, a gap is inserted
        int64_t time_threshold{-1};  // -1 = disabled
        
        /// Maximum allowed value change between samples
        /// If exceeded, a gap is inserted
        float value_threshold{-1.0f};  // -1 = disabled
        
        /// Minimum segment length (in samples)
        /// Segments shorter than this are discarded
        int min_segment_length{2};
    };

    GapDetector() = default;
    explicit GapDetector(Config const& config);
    
    /**
     * @brief Set time-based gap threshold
     * @param threshold Maximum time between samples
     */
    void setTimeThreshold(int64_t threshold);
    
    /**
     * @brief Set value-based gap threshold
     * @param threshold Maximum value change between samples
     */
    void setValueThreshold(float threshold);
    
    /**
     * @brief Set minimum segment length
     * @param length Minimum number of samples per segment
     */
    void setMinSegmentLength(int length);
    
    /**
     * @brief Get current configuration
     */
    [[nodiscard]] Config const& getConfig() const { return _config; }
    
    /**
     * @brief Transform analog time series into segmented poly-line batch
     * 
     * @param series Source analog time series
     * @param time_frame Time frame for converting indices to time
     * @param entity_id EntityId for all segments (optional)
     * @return Renderable batch with segmented geometry
     */
    [[nodiscard]] RenderablePolyLineBatch transform(
        AnalogTimeSeries const& series,
        TimeFrame const& time_frame,
        EntityId entity_id = EntityId(0)) const;
    
    /**
     * @brief Transform with explicit time and value arrays
     * 
     * Useful when data is already in array form or for testing.
     * 
     * @param time_values Time values for each sample
     * @param data_values Data values for each sample
     * @param entity_id EntityId for all segments (optional)
     * @return Renderable batch with segmented geometry
     */
    [[nodiscard]] RenderablePolyLineBatch transform(
        std::vector<float> const& time_values,
        std::vector<float> const& data_values,
        EntityId entity_id = EntityId(0)) const;

private:
    Config _config;
    
    /**
     * @brief Detect gap between two consecutive samples
     * @return true if gap should be inserted
     */
    [[nodiscard]] bool detectGap(
        float time1, float time2,
        float value1, float value2) const;
};

} // namespace CorePlotting

#endif // COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP
