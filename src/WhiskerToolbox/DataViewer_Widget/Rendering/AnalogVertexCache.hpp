#ifndef DATAVIEWER_WIDGET_ANALOGVERTEXCACHE_HPP
#define DATAVIEWER_WIDGET_ANALOGVERTEXCACHE_HPP

/**
 * @file AnalogVertexCache.hpp
 * @brief Ring buffer-based vertex caching for efficient scrolling of analog time series
 * 
 * This cache dramatically improves rendering performance when scrolling time series data.
 * Instead of regenerating all vertices each frame, it maintains a ring buffer of cached
 * vertices and only generates the new edge data when scrolling.
 * 
 * ## Performance Impact
 * 
 * For typical scrolling scenarios (100K visible points, scroll by 10-100 points):
 * - Without cache: ~1.3ms to regenerate all vertices
 * - With cache: ~10-50µs to generate only new edge data (26-130x faster)
 * 
 * ## Usage
 * 
 * @code
 *   AnalogVertexCache cache;
 *   cache.initialize(visible_points * 3);  // 3x capacity for margin
 *   
 *   // On each frame:
 *   if (cache.needsUpdate(new_start, new_end)) {
 *       auto missing = cache.getMissingRanges(new_start, new_end);
 *       // Generate vertices only for missing ranges
 *       for (auto& range : missing) {
 *           auto vertices = generateVertices(series, range.start, range.end);
 *           cache.appendVertices(vertices, range.start < cache.cachedStart());
 *       }
 *   }
 *   // Extract visible portion
 *   auto visible = cache.getVerticesForRange(new_start, new_end);
 * @endcode
 * 
 * @see streaming_renderer_analysis.md for design rationale
 */

#include "TimeFrame/TimeFrame.hpp"

#include <boost/circular_buffer.hpp>
#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace DataViewer {

/**
 * @brief A cached vertex with time index for range tracking
 */
struct CachedAnalogVertex {
    float x;                 ///< Time coordinate (in master time frame units)
    float y;                 ///< Data value
    TimeFrameIndex time_idx; ///< Original time index for range tracking
};

/**
 * @brief Describes a time range that needs vertex generation
 */
struct MissingRange {
    TimeFrameIndex start;
    TimeFrameIndex end;
    bool prepend;  ///< true = add to front (scrolling left), false = add to back (scrolling right)
};

/**
 * @brief Ring buffer-based cache for analog series vertices
 * 
 * Maintains a circular buffer of vertices with tracking of the cached time range.
 * Automatically discards old data when new data is pushed, enabling efficient
 * incremental updates during scrolling.
 */
class AnalogVertexCache {
public:
    AnalogVertexCache() = default;
    
    /**
     * @brief Initialize the cache with a given capacity
     * 
     * Recommended capacity is 2-3x the visible window size to allow
     * for smooth scrolling in both directions.
     * 
     * @param capacity Maximum number of vertices to cache
     */
    void initialize(size_t capacity);
    
    /**
     * @brief Clear all cached data and reset state
     */
    void invalidate();
    
    /**
     * @brief Check if the cache has been initialized
     */
    [[nodiscard]] bool isInitialized() const { return m_capacity > 0; }
    
    /**
     * @brief Check if the cache contains valid data
     */
    [[nodiscard]] bool isValid() const { return m_valid; }
    
    /**
     * @brief Get the currently cached time range
     */
    [[nodiscard]] TimeFrameIndex cachedStart() const { return m_cached_start; }
    [[nodiscard]] TimeFrameIndex cachedEnd() const { return m_cached_end; }
    
    /**
     * @brief Check if the requested range is fully covered by the cache
     * 
     * @param start Start of requested range (inclusive)
     * @param end End of requested range (exclusive)
     * @return true if the entire range is cached
     */
    [[nodiscard]] bool covers(TimeFrameIndex start, TimeFrameIndex end) const;
    
    /**
     * @brief Check if the cache needs updating for a new time range
     * 
     * Returns true if:
     * - Cache is invalid/empty
     * - Requested range extends beyond cached range
     * - Requested range has no overlap (jumped to new location)
     * 
     * @param start Start of requested range
     * @param end End of requested range
     */
    [[nodiscard]] bool needsUpdate(TimeFrameIndex start, TimeFrameIndex end) const;
    
    /**
     * @brief Calculate which ranges need to be generated
     * 
     * For scrolling scenarios, this typically returns 0-2 ranges:
     * - Scrolling right: one range at the end
     * - Scrolling left: one range at the beginning
     * - No scroll: empty vector (fully cached)
     * - Large jump: single range covering the whole request (cache miss)
     * 
     * @param start Start of requested range
     * @param end End of requested range
     * @return Vector of ranges that need vertex generation
     */
    [[nodiscard]] std::vector<MissingRange> getMissingRanges(TimeFrameIndex start, TimeFrameIndex end) const;
    
    /**
     * @brief Add vertices to the front of the cache (for scrolling left)
     * 
     * The vertices should be in ascending time order.
     * Old vertices at the back may be discarded if capacity is exceeded.
     * 
     * @param vertices Vertices to prepend
     */
    void prependVertices(std::vector<CachedAnalogVertex> const& vertices);
    
    /**
     * @brief Add vertices to the back of the cache (for scrolling right)
     * 
     * The vertices should be in ascending time order.
     * Old vertices at the front may be discarded if capacity is exceeded.
     * 
     * @param vertices Vertices to append
     */
    void appendVertices(std::vector<CachedAnalogVertex> const& vertices);
    
    /**
     * @brief Replace all cached vertices (for cache misses or initialization)
     * 
     * @param vertices New vertex data
     * @param start Start time index of the data
     * @param end End time index of the data
     */
    void setVertices(std::vector<CachedAnalogVertex> const& vertices,
                     TimeFrameIndex start, TimeFrameIndex end);
    
    /**
     * @brief Extract vertices for a specific time range
     * 
     * Returns a flat float array suitable for GPU upload: [x0, y0, x1, y1, ...]
     * 
     * @param start Start of range to extract (inclusive)
     * @param end End of range to extract (exclusive)
     * @return Flat vertex array, or empty if range not cached
     */
    [[nodiscard]] std::vector<float> getVerticesForRange(TimeFrameIndex start, TimeFrameIndex end) const;
    
    /**
     * @brief Get statistics about cache usage
     */
    [[nodiscard]] size_t size() const { return m_vertices.size(); }
    [[nodiscard]] size_t capacity() const { return m_capacity; }
    [[nodiscard]] float utilizationRatio() const { 
        return m_capacity > 0 ? static_cast<float>(m_vertices.size()) / static_cast<float>(m_capacity) : 0.0f;
    }

private:
    boost::circular_buffer<CachedAnalogVertex> m_vertices;
    size_t m_capacity{0};
    TimeFrameIndex m_cached_start{0};
    TimeFrameIndex m_cached_end{0};
    bool m_valid{false};
    
    /**
     * @brief Find the index in the buffer for a given time index
     * 
     * Uses binary search since vertices are sorted by time.
     * @return Index in buffer, or -1 if not found
     */
    [[nodiscard]] std::ptrdiff_t findIndexForTime(TimeFrameIndex time_idx) const;
};

} // namespace DataViewer

#endif // DATAVIEWER_WIDGET_ANALOGVERTEXCACHE_HPP
