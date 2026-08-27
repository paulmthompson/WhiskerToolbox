#include "AnalogVertexCache.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <ranges>

namespace DataViewer {

void AnalogVertexCache::initialize(size_t capacity) {
    if (m_capacity != capacity && m_capacity > 0) {
        spdlog::debug("AnalogVertexCache: Cache wiped and re-initialized. Capacity changing from {} to {}", m_capacity, capacity);
    }
    m_capacity = capacity;
    m_vertices.set_capacity(capacity);
    invalidate();
}

void AnalogVertexCache::invalidate() {
    m_vertices.clear();
    m_cached_start = TimeFrameIndex{0};
    m_cached_end = TimeFrameIndex{0};
    m_valid = false;
}

bool AnalogVertexCache::covers(TimeFrameIndex start, TimeFrameIndex end) const {
    if (!m_valid || m_vertices.empty()) {
        return false;
    }
    return start >= m_cached_start && end <= m_cached_end;
}

bool AnalogVertexCache::needsUpdate(TimeFrameIndex start, TimeFrameIndex end) const {
    if (!m_valid || m_vertices.empty()) {
        return true;
    }

    // Check if there's any overlap
    if (end <= m_cached_start || start >= m_cached_end) {
        // No overlap - cache miss
        return true;
    }

    // Check if we need more data at either end
    return start < m_cached_start || end > m_cached_end;
}

std::vector<MissingRange> AnalogVertexCache::getMissingRanges(TimeFrameIndex start, TimeFrameIndex end) const {
    std::vector<MissingRange> result;

    if (!m_valid || m_vertices.empty()) {
        // Complete cache miss - need entire range
        result.push_back({start, end, false});
        return result;
    }

    // Check for no overlap (jumped to completely different location)
    if (end <= m_cached_start || start >= m_cached_end) {
        // Complete cache miss
        result.push_back({start, end, false});
        return result;
    }

    // Check if we need data at the beginning (scrolling left)
    if (start < m_cached_start) {
        result.push_back({start, m_cached_start, true});
    }

    // Check if we need data at the end (scrolling right)
    if (end > m_cached_end) {
        result.push_back({m_cached_end, end, false});
    }

    return result;
}

void AnalogVertexCache::prependVertices(std::vector<CachedAnalogVertex> const & vertices, TimeFrameIndex requested_start) {

    bool const was_empty = vertices.empty();
    // Insert at front - circular buffer handles overflow automatically
    // We need to insert in reverse order since push_front will reverse them
    for (auto vertice: std::ranges::reverse_view(vertices)) {
        m_vertices.push_front(vertice);
    }

    // Update cached range
    m_cached_start = was_empty ? requested_start : vertices.front().time_idx;
    if (!m_valid) {
        m_cached_end = vertices.back().time_idx + TimeFrameIndex{1};
    }
    m_valid = true;

    // Update end if buffer overflowed (oldest data was discarded)
    if (!m_vertices.empty()) {
        m_cached_end = m_vertices.back().time_idx + TimeFrameIndex{1};
    }
}

void AnalogVertexCache::appendVertices(std::vector<CachedAnalogVertex> const & vertices, TimeFrameIndex requested_end) {
    if (vertices.empty()) {
        // Preserve prior behavior: advance the exclusive end even when no geometry arrives,
        // so callers do not repeatedly query the same empty tail.
        if (m_valid) {
            m_cached_end = requested_end;
        }
        return;
    }

    for (auto const & v: vertices) {
        m_vertices.push_back(v);
    }

    if (!m_valid) {
        m_cached_start = vertices.front().time_idx;
    }
    m_valid = true;

    if (!m_vertices.empty()) {
        m_cached_start = m_vertices.front().time_idx;
        m_cached_end = m_vertices.back().time_idx + TimeFrameIndex{1};
    }
}

void AnalogVertexCache::setVertices(std::vector<CachedAnalogVertex> const & vertices,
                                    TimeFrameIndex start, TimeFrameIndex end) {
    m_vertices.clear();

    if (vertices.empty()) {
        m_valid = false;
        return;
    }

    // Copy all vertices (circular buffer will limit to capacity if needed)
    for (auto const & v: vertices) {
        m_vertices.push_back(v);
    }

    // If the buffer is empty after pushing (e.g. capacity was zero), bail out
    if (m_vertices.empty()) {
        m_valid = false;
        return;
    }

    // AnalogTimeSeries range queries treat end_time as inclusive, but cache
    // bookkeeping and getVerticesForRange() use half-open [cached_start, cached_end).
    // Always record the actual stored vertex extent so incremental missing ranges do not
    // regenerate the trailing sample (duplicate time_idx corrupts extraction vs. mapper).
    m_cached_start = m_vertices.front().time_idx;
    m_cached_end = m_vertices.back().time_idx + TimeFrameIndex{1};

    static_cast<void>(start);
    static_cast<void>(end);

    m_valid = true;
}

std::vector<float> AnalogVertexCache::getVerticesForRange(TimeFrameIndex start,
                                                          TimeFrameIndex end,
                                                          ClockTicks x_origin_master_absolute_time) const {
    std::vector<float> result;

    if (!m_valid || m_vertices.empty()) {
        return result;
    }

    // Find start position using binary search
    auto const start_idx = findIndexForTime(start);
    if (start_idx < 0) {
        return result;// Start not found
    }

    // Extract vertices in range
    result.reserve(static_cast<size_t>(end.getValue() - start.getValue()) * 2);

    for (auto i = static_cast<size_t>(start_idx); i < m_vertices.size(); ++i) {
        auto const & v = m_vertices[i];
        if (v.time_idx >= end) {
            break;
        }
        auto const t_val = static_cast<int32_t>(v.x.getValue());
        auto const t_raw = std::bit_cast<float>(t_val);
        result.push_back(t_raw);
        result.push_back(v.y);
    }

    static_cast<void>(x_origin_master_absolute_time);

    return result;
}

std::vector<float> AnalogVertexCache::getVerticesForRangeDecimated(
        TimeFrameIndex start,
        TimeFrameIndex end,
        int bucket_count,
        ClockTicks x_origin_master_absolute_time) const {
    if (!m_valid || m_vertices.empty() || bucket_count <= 0) {
        return getVerticesForRange(start, end, x_origin_master_absolute_time);
    }

    auto const start_idx = findIndexForTime(start);
    if (start_idx < 0) {
        return {};
    }

    // Find upper bound index in circular buffer
    auto it_end = std::lower_bound(
            m_vertices.begin() + start_idx, m_vertices.end(), end,
            [](CachedAnalogVertex const & v, TimeFrameIndex t) {
                return v.time_idx < t;
            });
    size_t const end_idx = static_cast<size_t>(std::distance(m_vertices.begin(), it_end));
    size_t const total_samples = (end_idx > static_cast<size_t>(start_idx)) ? (end_idx - static_cast<size_t>(start_idx)) : 0;

    if (total_samples <= static_cast<size_t>(bucket_count * 2)) {
        return getVerticesForRange(start, end, x_origin_master_absolute_time);
    }

    std::vector<float> result;
    result.reserve(static_cast<size_t>(bucket_count) * 4 + 4);

    size_t const B = static_cast<size_t>(bucket_count);

    auto appendDedupe = [&](int32_t t_int, float y) {
        float const t_raw = std::bit_cast<float>(t_int);
        if (result.size() >= 2) {
            float const prev_t_raw = result[result.size() - 2];
            float const prev_y = result[result.size() - 1];
            if (std::bit_cast<int32_t>(prev_t_raw) == t_int && std::abs(prev_y - y) <= 1e-7f) {
                return;
            }
        }
        result.push_back(t_raw);
        result.push_back(y);
    };

    // Always append first vertex
    auto const & first_v = m_vertices[static_cast<size_t>(start_idx)];
    appendDedupe(static_cast<int32_t>(first_v.x.getValue()), first_v.y);

    for (size_t b = 0; b < B; ++b) {
        size_t const chunk_start = static_cast<size_t>(start_idx) + (b * total_samples) / B;
        size_t const chunk_end = static_cast<size_t>(start_idx) + ((b + 1) * total_samples) / B;

        if (chunk_start >= chunk_end) {
            continue;
        }

        size_t min_idx = chunk_start;
        size_t max_idx = chunk_start;
        float min_val = m_vertices[chunk_start].y;
        float max_val = m_vertices[chunk_start].y;

        for (size_t k = chunk_start + 1; k < chunk_end; ++k) {
            float const y = m_vertices[k].y;
            if (y < min_val) {
                min_val = y;
                min_idx = k;
            }
            if (y > max_val) {
                max_val = y;
                max_idx = k;
            }
        }

        // Emit in chronological order
        if (min_idx <= max_idx) {
            auto const & v1 = m_vertices[min_idx];
            appendDedupe(static_cast<int32_t>(v1.x.getValue()), v1.y);
            if (min_idx != max_idx) {
                auto const & v2 = m_vertices[max_idx];
                appendDedupe(static_cast<int32_t>(v2.x.getValue()), v2.y);
            }
        } else {
            auto const & v1 = m_vertices[max_idx];
            appendDedupe(static_cast<int32_t>(v1.x.getValue()), v1.y);
            auto const & v2 = m_vertices[min_idx];
            appendDedupe(static_cast<int32_t>(v2.x.getValue()), v2.y);
        }
    }

    // Always append last vertex
    auto const & last_v = m_vertices[end_idx - 1];
    appendDedupe(static_cast<int32_t>(last_v.x.getValue()), last_v.y);

    static_cast<void>(x_origin_master_absolute_time);
    return result;
}

std::ptrdiff_t AnalogVertexCache::findIndexForTime(TimeFrameIndex time_idx) const {
    if (m_vertices.empty()) {
        return -1;
    }

    // Binary search for the time index
    // Note: circular_buffer iterators support random access
    auto it = std::lower_bound(
            m_vertices.begin(), m_vertices.end(), time_idx,
            [](CachedAnalogVertex const & v, TimeFrameIndex t) {
                return v.time_idx < t;
            });

    if (it == m_vertices.end()) {
        return -1;
    }

    return std::distance(m_vertices.begin(), it);
}

}// namespace DataViewer
