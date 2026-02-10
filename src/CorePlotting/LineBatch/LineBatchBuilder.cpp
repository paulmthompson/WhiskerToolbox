/**
 * @file LineBatchBuilder.cpp
 * @brief Builder implementations for LineBatchData from LineData and GatherResult
 */
#include "LineBatchBuilder.hpp"

#include <algorithm>
#include <cstddef>

namespace CorePlotting {

// ── buildLineBatchFromLineData ─────────────────────────────────────────

LineBatchData buildLineBatchFromLineData(
    LineData const & line_data,
    float canvas_width,
    float canvas_height)
{
    LineBatchData batch;
    batch.canvas_width = canvas_width;
    batch.canvas_height = canvas_height;

    std::uint32_t line_id = 0; // 1-based after increment

    for (auto elem : line_data.elementsView()) {
        Line2D const & line = elem.data();
        EntityId const eid = elem.id();

        if (line.size() < 2) {
            continue; // Need at least 2 points to form a segment
        }

        ++line_id;
        std::uint32_t const first_seg = batch.numSegments();
        std::uint32_t seg_count = 0;

        for (std::size_t i = 0; i + 1 < line.size(); ++i) {
            auto const p0 = line[i];
            auto const p1 = line[i + 1];

            batch.segments.push_back(p0.x);
            batch.segments.push_back(p0.y);
            batch.segments.push_back(p1.x);
            batch.segments.push_back(p1.y);

            batch.line_ids.push_back(line_id);
            ++seg_count;
        }

        LineBatchData::LineInfo info;
        info.entity_id = eid;
        info.trial_index = 0;
        info.first_segment = first_seg;
        info.segment_count = seg_count;
        batch.lines.push_back(info);
    }

    // All lines visible, none selected
    batch.visibility_mask.assign(batch.numLines(), 1);
    batch.selection_mask.assign(batch.numLines(), 0);

    return batch;
}

// ── buildLineBatchFromGatherResult ─────────────────────────────────────

LineBatchData buildLineBatchFromGatherResult(
    GatherResult<AnalogTimeSeries> const & gathered,
    std::vector<std::int64_t> const & alignment_times)
{
    LineBatchData batch;
    batch.canvas_width = 1.0f;
    batch.canvas_height = 1.0f;

    std::uint32_t line_id = 0;

    for (std::size_t trial = 0; trial < gathered.size(); ++trial) {
        auto const & series = gathered[trial];
        if (!series || series->getNumSamples() < 2) {
            continue;
        }

        ++line_id;
        std::uint32_t const first_seg = batch.numSegments();
        std::uint32_t seg_count = 0;

        std::int64_t const align =
            trial < alignment_times.size() ? alignment_times[trial] : 0;

        // Iterate time-value pairs via the lazy view
        auto trial_view = series->view();

        // We need to read pairs of consecutive points.
        // Materialise a lightweight buffer of (x,y) for this trial.
        struct XY { float x; float y; };
        std::vector<XY> pts;
        pts.reserve(series->getNumSamples());

        for (auto tvp : trial_view) {
            float const x = static_cast<float>(tvp.time().getValue() - align);
            float const y = tvp.value();
            pts.push_back({x, y});
        }

        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            batch.segments.push_back(pts[i].x);
            batch.segments.push_back(pts[i].y);
            batch.segments.push_back(pts[i + 1].x);
            batch.segments.push_back(pts[i + 1].y);

            batch.line_ids.push_back(line_id);
            ++seg_count;
        }

        LineBatchData::LineInfo info;
        info.entity_id = EntityId{0};
        info.trial_index = static_cast<std::uint32_t>(trial);
        info.first_segment = first_seg;
        info.segment_count = seg_count;
        batch.lines.push_back(info);
    }

    batch.visibility_mask.assign(batch.numLines(), 1);
    batch.selection_mask.assign(batch.numLines(), 0);

    return batch;
}

} // namespace CorePlotting
