#include "AnalogVertexCache.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <ranges>

namespace DataViewer {

void AnalogVertexCache::initialize(size_t capacity) {
    if (m_capacity != capacity && m_capacity > 0) {
        spdlog::debug("AnalogVertexCache: Cache wiped and re-initialized. Capacity changing from {} to {}", m_capacity, capacity);
    }
    m_capacity = capacity;
    m_time_indices.set_capacity(capacity);
    m_flat_data.set_capacity(capacity * 2);
    invalidate();
}

void AnalogVertexCache::invalidate() {
    m_time_indices.clear();
    m_flat_data.clear();
    m_cached_start = TimeFrameIndex{0};
    m_cached_end = TimeFrameIndex{0};
    m_valid = false;
}

bool AnalogVertexCache::covers(TimeFrameIndex start, TimeFrameIndex end) const {
    if (!m_valid || m_time_indices.empty()) {
        return false;
    }
    return start >= m_cached_start && end <= m_cached_end;
}

bool AnalogVertexCache::needsUpdate(TimeFrameIndex start, TimeFrameIndex end) const {
    if (!m_valid || m_time_indices.empty()) {
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

    if (!m_valid || m_time_indices.empty()) {
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
    for (auto const & v: std::ranges::reverse_view(vertices)) {
        int32_t const t_int = static_cast<int32_t>(v.x.getValue());
        float const t_raw = std::bit_cast<float>(t_int);
        m_time_indices.push_front(v.time_idx);
        m_flat_data.push_front(v.y);
        m_flat_data.push_front(t_raw);
    }

    // Update cached range
    m_cached_start = was_empty ? requested_start : vertices.front().time_idx;
    if (!m_valid) {
        m_cached_end = vertices.back().time_idx + TimeFrameIndex{1};
    }
    m_valid = true;

    // Update end if buffer overflowed (oldest data was discarded)
    if (!m_time_indices.empty()) {
        m_cached_end = m_time_indices.back() + TimeFrameIndex{1};
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
        int32_t const t_int = static_cast<int32_t>(v.x.getValue());
        float const t_raw = std::bit_cast<float>(t_int);
        m_time_indices.push_back(v.time_idx);
        m_flat_data.push_back(t_raw);
        m_flat_data.push_back(v.y);
    }

    if (!m_valid) {
        m_cached_start = vertices.front().time_idx;
    }
    m_valid = true;

    if (!m_time_indices.empty()) {
        m_cached_start = m_time_indices.front();
        m_cached_end = m_time_indices.back() + TimeFrameIndex{1};
    }
}

void AnalogVertexCache::setVertices(std::vector<CachedAnalogVertex> const & vertices,
                                    TimeFrameIndex start, TimeFrameIndex end) {
    m_time_indices.clear();
    m_flat_data.clear();

    if (vertices.empty()) {
        m_valid = false;
        return;
    }

    // Copy all vertices into flat storage
    for (auto const & v: vertices) {
        int32_t const t_int = static_cast<int32_t>(v.x.getValue());
        float const t_raw = std::bit_cast<float>(t_int);
        m_time_indices.push_back(v.time_idx);
        m_flat_data.push_back(t_raw);
        m_flat_data.push_back(v.y);
    }

    // If the buffer is empty after pushing (e.g. capacity was zero), bail out
    if (m_time_indices.empty()) {
        m_valid = false;
        return;
    }

    m_cached_start = m_time_indices.front();
    m_cached_end = m_time_indices.back() + TimeFrameIndex{1};
    m_valid = true;

    static_cast<void>(start);
    static_cast<void>(end);
}

std::vector<float> AnalogVertexCache::getVerticesForRange(TimeFrameIndex start,
                                                          TimeFrameIndex end,
                                                          ClockTicks x_origin_master_absolute_time) const {
    std::vector<float> result;
    extractVerticesForRange(start, end, x_origin_master_absolute_time, result);
    return result;
}

void AnalogVertexCache::extractVerticesForRange(TimeFrameIndex start,
                                                TimeFrameIndex end,
                                                ClockTicks x_origin_master_absolute_time,
                                                std::vector<float> & out_buffer) const {
    out_buffer.clear();

    if (!m_valid || m_time_indices.empty()) {
        return;
    }

    // Binary search start index
    auto const it_start = std::lower_bound(m_time_indices.begin(), m_time_indices.end(), start);
    if (it_start == m_time_indices.end()) {
        return;
    }
    size_t const start_idx = static_cast<size_t>(std::distance(m_time_indices.begin(), it_start));

    // Binary search end index
    auto const it_end = std::lower_bound(it_start, m_time_indices.end(), end);
    size_t const end_idx = static_cast<size_t>(std::distance(m_time_indices.begin(), it_end));

    if (end_idx <= start_idx) {
        return;
    }

    size_t const vertex_count = end_idx - start_idx;
    size_t const float_count = vertex_count * 2;

    out_buffer.resize(float_count);

    size_t const offset = start_idx * 2;
    auto const arr1 = m_flat_data.array_one();
    auto const arr2 = m_flat_data.array_two();

    if (offset < arr1.second) {
        size_t const copy1 = std::min(float_count, arr1.second - offset);
        std::memcpy(out_buffer.data(), arr1.first + offset, copy1 * sizeof(float));
        if (copy1 < float_count) {
            size_t const copy2 = float_count - copy1;
            std::memcpy(out_buffer.data() + copy1, arr2.first, copy2 * sizeof(float));
        }
    } else {
        size_t const off2 = offset - arr1.second;
        std::memcpy(out_buffer.data(), arr2.first + off2, float_count * sizeof(float));
    }

    static_cast<void>(x_origin_master_absolute_time);
}

std::vector<float> AnalogVertexCache::getVerticesForRangeDecimated(
        TimeFrameIndex start,
        TimeFrameIndex end,
        int bucket_count,
        ClockTicks x_origin_master_absolute_time) const {
    std::vector<float> result;
    extractVerticesForRangeDecimated(start, end, bucket_count, x_origin_master_absolute_time, result);
    return result;
}

void AnalogVertexCache::extractVerticesForRangeDecimated(
        TimeFrameIndex start,
        TimeFrameIndex end,
        int bucket_count,
        ClockTicks x_origin_master_absolute_time,
        std::vector<float> & out_buffer) const {
    out_buffer.clear();

    if (!m_valid || m_time_indices.empty() || bucket_count <= 0) {
        extractVerticesForRange(start, end, x_origin_master_absolute_time, out_buffer);
        return;
    }

    auto const it_start = std::lower_bound(m_time_indices.begin(), m_time_indices.end(), start);
    if (it_start == m_time_indices.end()) {
        return;
    }
    size_t const start_idx = static_cast<size_t>(std::distance(m_time_indices.begin(), it_start));

    auto const it_end = std::lower_bound(it_start, m_time_indices.end(), end);
    size_t const end_idx = static_cast<size_t>(std::distance(m_time_indices.begin(), it_end));

    if (end_idx <= start_idx) {
        return;
    }

    size_t const total_samples = end_idx - start_idx;
    if (total_samples <= static_cast<size_t>(bucket_count * 2)) {
        extractVerticesForRange(start, end, x_origin_master_absolute_time, out_buffer);
        return;
    }

    out_buffer.reserve(static_cast<size_t>(bucket_count) * 4 + 4);

    size_t const B = static_cast<size_t>(bucket_count);

    auto const arr1 = m_flat_data.array_one();
    auto const arr2 = m_flat_data.array_two();

    auto getSample = [&](size_t idx) -> std::pair<float, float> {
        size_t const float_idx = idx * 2;
        if (float_idx < arr1.second) {
            return {arr1.first[float_idx], arr1.first[float_idx + 1]};
        } else {
            size_t const off2 = float_idx - arr1.second;
            return {arr2.first[off2], arr2.first[off2 + 1]};
        }
    };

    auto appendDedupe = [&](float t_raw, float y) {
        if (out_buffer.size() >= 2) {
            float const prev_t_raw = out_buffer[out_buffer.size() - 2];
            float const prev_y = out_buffer[out_buffer.size() - 1];
            if (std::bit_cast<int32_t>(prev_t_raw) == std::bit_cast<int32_t>(t_raw) && std::abs(prev_y - y) <= 1e-7f) {
                return;
            }
        }
        out_buffer.push_back(t_raw);
        out_buffer.push_back(y);
    };

    // Always append first vertex
    auto const first_v = getSample(start_idx);
    appendDedupe(first_v.first, first_v.second);

    for (size_t b = 0; b < B; ++b) {
        size_t const chunk_start = start_idx + (b * total_samples) / B;
        size_t const chunk_end = start_idx + ((b + 1) * total_samples) / B;

        if (chunk_start >= chunk_end) {
            continue;
        }

        size_t min_idx = chunk_start;
        size_t max_idx = chunk_start;
        auto const start_s = getSample(chunk_start);
        float min_val = start_s.second;
        float max_val = start_s.second;

        size_t const start_float = chunk_start * 2;
        size_t const end_float = chunk_end * 2;

        if (end_float <= arr1.second) {
            // Contiguous fast-path in arr1
            float const * const p = arr1.first;
            for (size_t k = chunk_start + 1; k < chunk_end; ++k) {
                float const y = p[k * 2 + 1];
                if (y < min_val) {
                    min_val = y;
                    min_idx = k;
                }
                if (y > max_val) {
                    max_val = y;
                    max_idx = k;
                }
            }
        } else if (start_float >= arr1.second) {
            // Contiguous fast-path in arr2
            size_t const arr1_samples = arr1.second / 2;
            float const * const p = arr2.first;
            for (size_t k = chunk_start + 1; k < chunk_end; ++k) {
                float const y = p[(k - arr1_samples) * 2 + 1];
                if (y < min_val) {
                    min_val = y;
                    min_idx = k;
                }
                if (y > max_val) {
                    max_val = y;
                    max_idx = k;
                }
            }
        } else {
            // Straddles boundary (rare)
            for (size_t k = chunk_start + 1; k < chunk_end; ++k) {
                auto const s = getSample(k);
                if (s.second < min_val) {
                    min_val = s.second;
                    min_idx = k;
                }
                if (s.second > max_val) {
                    max_val = s.second;
                    max_idx = k;
                }
            }
        }

        // Emit in chronological order
        if (min_idx <= max_idx) {
            auto const v1 = getSample(min_idx);
            appendDedupe(v1.first, v1.second);
            if (min_idx != max_idx) {
                auto const v2 = getSample(max_idx);
                appendDedupe(v2.first, v2.second);
            }
        } else {
            auto const v1 = getSample(max_idx);
            appendDedupe(v1.first, v1.second);
            auto const v2 = getSample(min_idx);
            appendDedupe(v2.first, v2.second);
        }
    }

    // Always append last vertex
    auto const last_v = getSample(end_idx - 1);
    appendDedupe(last_v.first, last_v.second);

    static_cast<void>(x_origin_master_absolute_time);
}

std::ptrdiff_t AnalogVertexCache::findIndexForTime(TimeFrameIndex time_idx) const {
    if (m_time_indices.empty()) {
        return -1;
    }

    auto it = std::lower_bound(m_time_indices.begin(), m_time_indices.end(), time_idx);
    if (it == m_time_indices.end()) {
        return -1;
    }

    return std::distance(m_time_indices.begin(), it);
}

}// namespace DataViewer
