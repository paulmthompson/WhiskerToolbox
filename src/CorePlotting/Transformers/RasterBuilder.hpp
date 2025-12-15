#ifndef COREPLOTTING_TRANSFORMERS_RASTERBUILDER_HPP
#define COREPLOTTING_TRANSFORMERS_RASTERBUILDER_HPP

#include "Layout/LayoutEngine.hpp"
#include "SceneGraph/RenderablePrimitives.hpp"

#include "TimeFrame/TimeFrame.hpp"

class DigitalEventSeries;

namespace CorePlotting {

/**
 * @brief Builds raster plot visualizations from digital event series
 * 
 * Transforms DigitalEventSeries into RenderableGlyphBatch for raster plot display.
 * Each row represents a trial/condition, with events displayed as glyphs.
 * 
 * USAGE:
 * ```cpp
 * // Prepare layout (one row per trial)
 * LayoutRequest layout_request;
 * for (int trial = 0; trial < num_trials; ++trial) {
 *     layout_request.series.push_back({
 *         std::to_string(trial), SeriesType::DigitalEvent, true
 *     });
 * }
 * 
 * RowLayoutStrategy strategy;
 * LayoutResponse layout = strategy.compute(layout_request);
 * 
 * // Build raster glyphs
 * RasterBuilder builder;
 * RenderableGlyphBatch batch = builder.transform(
 *     event_series, time_frame, layout.layouts, trial_centers);
 * ```
 */
class RasterBuilder {
public:
    /**
     * @brief Configuration for raster visualization
     */
    struct Config {
        /// Glyph type for events
        RenderableGlyphBatch::GlyphType glyph_type{RenderableGlyphBatch::GlyphType::Tick};

        /// Glyph size
        float glyph_size{5.0f};

        /// Default color for events (if not using per-event colors)
        glm::vec4 default_color{1.0f, 1.0f, 1.0f, 1.0f};

        /// Time window relative to each row center
        /// Events outside [center + window_start, center + window_end] are excluded
        int64_t window_start{-1000};
        int64_t window_end{1000};
    };

    RasterBuilder() = default;
    explicit RasterBuilder(Config const & config);

    /**
     * @brief Set glyph type
     */
    void setGlyphType(RenderableGlyphBatch::GlyphType type);

    /**
     * @brief Set glyph size
     */
    void setGlyphSize(float size);

    /**
     * @brief Set time window
     */
    void setTimeWindow(int64_t start, int64_t end);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] Config const & getConfig() const { return _config; }

    /**
     * @brief Transform event series into raster glyph batch
     * 
     * @param series Source digital event series
     * @param time_frame Time frame for converting indices to time
     * @param row_layouts Layout positions for each row
     * @param row_centers Time center for each row (typically trial start times)
     * @return Renderable glyph batch
     */
    [[nodiscard]] RenderableGlyphBatch transform(
            DigitalEventSeries const & series,
            TimeFrame const & time_frame,
            std::vector<SeriesLayout> const & row_layouts,
            std::vector<int64_t> const & row_centers) const;

    /**
     * @brief Transform with explicit event times and entity IDs
     * 
     * Useful for testing or when event data is already processed.
     * 
     * @param event_times Absolute time for each event
     * @param event_ids EntityId for each event
     * @param row_layouts Layout positions for each row
     * @param row_centers Time center for each row
     * @return Renderable glyph batch
     */
    [[nodiscard]] RenderableGlyphBatch transform(
            std::vector<int64_t> const & event_times,
            std::vector<EntityId> const & event_ids,
            std::vector<SeriesLayout> const & row_layouts,
            std::vector<int64_t> const & row_centers) const;

private:
    Config _config;

    /**
     * @brief Check if event time is within window relative to row center
     */
    [[nodiscard]] bool isInWindow(int64_t event_time, int64_t row_center) const;
};

}// namespace CorePlotting

#endif// COREPLOTTING_TRANSFORMERS_RASTERBUILDER_HPP
