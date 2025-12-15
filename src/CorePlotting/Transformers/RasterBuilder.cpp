#include "RasterBuilder.hpp"

namespace CorePlotting {

RasterBuilder::RasterBuilder(Config const& config)
    : _config(config) {}

void RasterBuilder::setGlyphType(RenderableGlyphBatch::GlyphType type) {
    _config.glyph_type = type;
}

void RasterBuilder::setGlyphSize(float size) {
    _config.glyph_size = size;
}

void RasterBuilder::setTimeWindow(int64_t start, int64_t end) {
    _config.window_start = start;
    _config.window_end = end;
}

bool RasterBuilder::isInWindow(int64_t event_time, int64_t row_center) const {
    int64_t const relative_time = event_time - row_center;
    return (relative_time >= _config.window_start && relative_time <= _config.window_end);
}

RenderableGlyphBatch RasterBuilder::transform(
    DigitalEventSeries const& series,
    TimeFrame const& time_frame,
    std::vector<SeriesLayout> const& row_layouts,
    std::vector<int64_t> const& row_centers) const {
    
    // Get all events with their entity IDs
    auto const event_view = series.view();
    
    std::vector<int64_t> event_times;
    std::vector<EntityId> event_ids;
    
    for (auto const& event : event_view) {
        int const time = time_frame.getTimeAtIndex(event.event_time);
        event_times.push_back(static_cast<int64_t>(time));
        event_ids.push_back(event.entity_id);
    }
    
    return transform(event_times, event_ids, row_layouts, row_centers);
}

RenderableGlyphBatch RasterBuilder::transform(
    std::vector<int64_t> const& event_times,
    std::vector<EntityId> const& event_ids,
    std::vector<SeriesLayout> const& row_layouts,
    std::vector<int64_t> const& row_centers) const {
    
    RenderableGlyphBatch batch;
    batch.glyph_type = _config.glyph_type;
    batch.size = _config.glyph_size;
    
    if (event_times.size() != event_ids.size()) {
        return batch;  // Empty batch on size mismatch
    }
    
    if (row_layouts.size() != row_centers.size()) {
        return batch;  // Empty batch on size mismatch
    }
    
    // Reserve space (worst case: all events appear in all rows)
    size_t const max_glyphs = event_times.size() * row_centers.size();
    batch.positions.reserve(max_glyphs);
    batch.colors.reserve(max_glyphs);
    batch.entity_ids.reserve(max_glyphs);
    
    // For each row, place events that fall within time window
    for (size_t row_idx = 0; row_idx < row_centers.size(); ++row_idx) {
        int64_t const row_center = row_centers[row_idx];
        float const row_y = row_layouts[row_idx].result.allocated_y_center;
        
        // Find events in window for this row
        for (size_t event_idx = 0; event_idx < event_times.size(); ++event_idx) {
            int64_t const event_time = event_times[event_idx];
            
            if (!isInWindow(event_time, row_center)) {
                continue;  // Skip events outside window
            }
            
            // Calculate relative time (x position)
            float const relative_time = static_cast<float>(event_time - row_center);
            
            // Add glyph
            batch.positions.emplace_back(relative_time, row_y);
            batch.colors.push_back(_config.default_color);
            batch.entity_ids.push_back(event_ids[event_idx]);
        }
    }
    
    // Shrink to actual size
    batch.positions.shrink_to_fit();
    batch.colors.shrink_to_fit();
    batch.entity_ids.shrink_to_fit();
    
    return batch;
}

} // namespace CorePlotting
