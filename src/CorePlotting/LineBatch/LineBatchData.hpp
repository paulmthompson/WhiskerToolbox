/**
 * @file LineBatchData.hpp
 * @brief CPU-side batch line segment storage and topology
 *
 * Pure-data struct representing a batch of lines as flat segment arrays.
 * Populated from any data source (LineData, GatherResult<AnalogTimeSeries>);
 * consumed by both the CPU intersector and GPU BatchLineStore.
 *
 * Part of the CorePlotting layer — no OpenGL or Qt dependencies.
 */
#ifndef COREPLOTTING_LINEBATCH_LINEBATCHDATA_HPP
#define COREPLOTTING_LINEBATCH_LINEBATCHDATA_HPP

#include "Entity/EntityTypes.hpp"

#include <cstdint>
#include <vector>

namespace CorePlotting {

/// 0-based index into LineBatchData::lines
using LineBatchIndex = std::uint32_t;

/**
 * @brief Flat, API-agnostic representation of a batch of polylines as segments.
 *
 * Each polyline is decomposed into consecutive line segments stored in a flat
 * array.  Per-segment ownership and per-line metadata allow mapping selection
 * results back to the original data source (EntityId or trial index).
 */
struct LineBatchData {

    // ── Segment storage ────────────────────────────────────────────────
    /// Flat segment array: each segment is 4 consecutive floats {x1, y1, x2, y2}.
    /// Total size = numSegments() * 4.
    std::vector<float> segments;

    /// Per-segment line ownership (1-based line id; 0 = invalid).
    /// Size = numSegments().
    std::vector<std::uint32_t> line_ids;

    // ── Per-line metadata ──────────────────────────────────────────────
    /**
     * @brief Metadata for one logical line in the batch.
     *
     * Indexed by (line_id - 1).  Carries the information needed to map a
     * selection result back to the original data source.
     */
    struct LineInfo {
        EntityId entity_id{0};          ///< For LineData sources
        std::uint32_t trial_index{0};   ///< For GatherResult sources
        std::uint32_t first_segment{0}; ///< Start index into the segments array
        std::uint32_t segment_count{0}; ///< Number of segments belonging to this line
    };

    /// Per-line metadata array.  Size = numLines().
    std::vector<LineInfo> lines;

    // ── Per-line masks ─────────────────────────────────────────────────
    /// 1 = visible, 0 = hidden.  Size = numLines().
    std::vector<std::uint32_t> visibility_mask;

    /// 1 = selected, 0 = not selected.  Size = numLines().
    std::vector<std::uint32_t> selection_mask;

    // ── Canvas info ────────────────────────────────────────────────────
    float canvas_width{1.0f};
    float canvas_height{1.0f};

    // ── Queries ────────────────────────────────────────────────────────
    [[nodiscard]] std::uint32_t numSegments() const;
    [[nodiscard]] std::uint32_t numLines() const;
    [[nodiscard]] bool empty() const;

    // ── Mutators ───────────────────────────────────────────────────────
    void clear();
};

} // namespace CorePlotting

#endif // COREPLOTTING_LINEBATCH_LINEBATCHDATA_HPP
