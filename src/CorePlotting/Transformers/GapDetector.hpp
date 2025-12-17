#ifndef COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP
#define COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP

#include "CorePlotting/Mappers/MappedElement.hpp"
#include "SceneGraph/RenderablePrimitives.hpp"

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>

class AnalogTimeSeries;

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
 * 
 * Or with range-based API for mapped vertices:
 * ```cpp
 * auto mapped = TimeSeriesMapper::mapAnalogSeriesWithIndices(series, layout, tf, 1.0f, start, end);
 * auto batch = GapDetector::segmentByGaps(mapped, config);
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
        int64_t time_threshold{-1};// -1 = disabled

        /// Maximum allowed value change between samples
        /// If exceeded, a gap is inserted
        float value_threshold{-1.0f};// -1 = disabled

        /// Minimum segment length (in samples)
        /// Segments shorter than this are discarded
        int min_segment_length{2};
    };

    GapDetector() = default;
    explicit GapDetector(Config const & config);

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
    [[nodiscard]] Config const & getConfig() const { return _config; }

    /**
     * @brief Transform analog time series into segmented poly-line batch
     * 
     * @param series Source analog time series
     * @param time_frame Time frame for converting indices to time
     * @param entity_id EntityId for all segments (optional)
     * @return Renderable batch with segmented geometry
     */
    [[nodiscard]] RenderablePolyLineBatch transform(
            AnalogTimeSeries const & series,
            TimeFrame const & time_frame,
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
            std::vector<float> const & time_values,
            std::vector<float> const & data_values,
            EntityId entity_id = EntityId(0)) const;

    /**
     * @brief Segment a range of MappedAnalogVertex by detecting gaps
     * 
     * This is the preferred method for use with TimeSeriesMapper output.
     * Uses the time_index field of MappedAnalogVertex for gap detection.
     * 
     * @tparam Range Input range type (must yield MappedAnalogVertex)
     * @param vertices Range of MappedAnalogVertex to segment
     * @param config Gap detection configuration
     * @return Renderable batch with segmented geometry
     */
    template<std::ranges::input_range Range>
        requires std::same_as<std::ranges::range_value_t<Range>, MappedAnalogVertex>
    [[nodiscard]] static RenderablePolyLineBatch segmentByGaps(
            Range && vertices,
            Config const & config);

    /**
     * @brief Segment a vector of MappedAnalogVertex by detecting gaps
     * 
     * Convenience overload for materialized vectors.
     * 
     * @param vertices Vector of MappedAnalogVertex to segment
     * @param config Gap detection configuration
     * @return Renderable batch with segmented geometry
     */
    [[nodiscard]] static RenderablePolyLineBatch segmentByGaps(
            std::vector<MappedAnalogVertex> const & vertices,
            Config const & config);

private:
    Config _config;

    /**
     * @brief Detect gap between two consecutive samples
     * @return true if gap should be inserted
     */
    [[nodiscard]] bool detectGap(
            float time1, float time2,
            float value1, float value2) const;
    
    /**
     * @brief Detect gap between two MappedAnalogVertex based on time_index
     * @return true if gap should be inserted
     */
    [[nodiscard]] static bool detectGapByIndex(
            MappedAnalogVertex const & prev,
            MappedAnalogVertex const & curr,
            Config const & config);
};

// ============================================================================
// Template implementation
// ============================================================================

template<std::ranges::input_range Range>
    requires std::same_as<std::ranges::range_value_t<Range>, MappedAnalogVertex>
RenderablePolyLineBatch GapDetector::segmentByGaps(
        Range && vertices,
        Config const & config) {
    
    RenderablePolyLineBatch batch;
    
    std::vector<float> segment_vertices;
    MappedAnalogVertex prev_vertex{};
    bool first_point = true;
    int current_line_start = 0;
    
    auto finalize_segment = [&]() {
        if (segment_vertices.size() >= static_cast<size_t>(config.min_segment_length * 2)) {
            int const vertex_count = static_cast<int>(segment_vertices.size()) / 2;
            batch.line_start_indices.push_back(current_line_start);
            batch.line_vertex_counts.push_back(vertex_count);
            batch.vertices.insert(batch.vertices.end(),
                                  segment_vertices.begin(),
                                  segment_vertices.end());
            current_line_start += vertex_count;
        }
        segment_vertices.clear();
    };
    
    for (auto const & vertex : vertices) {
        // Check for gap if not first point
        if (!first_point && config.time_threshold > 0) {
            if (detectGapByIndex(prev_vertex, vertex, config)) {
                finalize_segment();
            }
        }
        
        // Add vertex to current segment
        segment_vertices.push_back(vertex.x);
        segment_vertices.push_back(vertex.y);
        
        prev_vertex = vertex;
        first_point = false;
    }
    
    // Finalize last segment
    finalize_segment();
    
    return batch;
}

}// namespace CorePlotting

#endif// COREPLOTTING_TRANSFORMERS_GAPDETECTOR_HPP
