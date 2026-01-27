#include "AnalogVertexCache.hpp"

#include <algorithm>
#include <cmath>

namespace DataViewer {

void AnalogVertexCache::initialize(size_t capacity) {
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

void AnalogVertexCache::prependVertices(std::vector<CachedAnalogVertex> const& vertices) {
    if (vertices.empty()) {
        return;
    }
    
    // Insert at front - circular buffer handles overflow automatically
    // We need to insert in reverse order since push_front will reverse them
    for (auto it = vertices.rbegin(); it != vertices.rend(); ++it) {
        m_vertices.push_front(*it);
    }
    
    // Update cached range
    m_cached_start = vertices.front().time_idx;
    if (!m_valid) {
        m_cached_end = vertices.back().time_idx + TimeFrameIndex{1};
    }
    m_valid = true;
    
    // Update end if buffer overflowed (oldest data was discarded)
    if (!m_vertices.empty()) {
        m_cached_end = m_vertices.back().time_idx + TimeFrameIndex{1};
    }
}

void AnalogVertexCache::appendVertices(std::vector<CachedAnalogVertex> const& vertices) {
    if (vertices.empty()) {
        return;
    }
    
    // Insert at back - circular buffer handles overflow automatically
    for (auto const& v : vertices) {
        m_vertices.push_back(v);
    }
    
    // Update cached range
    m_cached_end = vertices.back().time_idx + TimeFrameIndex{1};
    if (!m_valid) {
        m_cached_start = vertices.front().time_idx;
    }
    m_valid = true;
    
    // Update start if buffer overflowed (oldest data was discarded)
    if (!m_vertices.empty()) {
        m_cached_start = m_vertices.front().time_idx;
    }
}

void AnalogVertexCache::setVertices(std::vector<CachedAnalogVertex> const& vertices,
                                    TimeFrameIndex start, TimeFrameIndex end) {
    m_vertices.clear();
    
    if (vertices.empty()) {
        m_valid = false;
        return;
    }
    
    // Copy all vertices (circular buffer will limit to capacity if needed)
    for (auto const& v : vertices) {
        m_vertices.push_back(v);
    }
    
    // If we had to truncate, adjust the range
    if (m_vertices.size() < vertices.size()) {
        // Data was truncated - adjust start to match what we actually have
        m_cached_start = m_vertices.front().time_idx;
        m_cached_end = m_vertices.back().time_idx + TimeFrameIndex{1};
    } else {
        m_cached_start = start;
        m_cached_end = end;
    }
    
    m_valid = true;
}

std::vector<float> AnalogVertexCache::getVerticesForRange(TimeFrameIndex start, TimeFrameIndex end) const {
    std::vector<float> result;
    
    if (!m_valid || m_vertices.empty()) {
        return result;
    }
    
    // Find start position using binary search
    auto const start_idx = findIndexForTime(start);
    if (start_idx < 0) {
        return result;  // Start not found
    }
    
    // Extract vertices in range
    result.reserve(static_cast<size_t>(end.getValue() - start.getValue()) * 2);
    
    for (size_t i = static_cast<size_t>(start_idx); i < m_vertices.size(); ++i) {
        auto const& v = m_vertices[i];
        if (v.time_idx >= end) {
            break;
        }
        result.push_back(v.x);
        result.push_back(v.y);
    }
    
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
        [](CachedAnalogVertex const& v, TimeFrameIndex t) {
            return v.time_idx < t;
        });
    
    if (it == m_vertices.end()) {
        return -1;
    }
    
    return std::distance(m_vertices.begin(), it);
}

} // namespace DataViewer
