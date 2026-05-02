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
    float x;                ///< Time coordinate (in master time frame units)
    float y;                ///< Data value
    TimeFrameIndex time_idx;///< Original time index for range tracking
};

/**
 * @brief Describes a time range that needs vertex generation
 */
struct MissingRange {
    TimeFrameIndex start;
    TimeFrameIndex end;
    bool prepend;///< true = add to front (scrolling left), false = add to back (scrolling right)
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
     * Returns true only when the cache is valid, non-empty, and the entire
     * interval [@p start, @p end) lies within [cachedStart(), cachedEnd()).
     * Returns false (conservatively) when the cache is invalid or empty.
     *
     * This function is purely computational: no memory allocation, no
     * dereferences, safe to call regardless of cache validity.
     *
     * @param start Start of requested range (inclusive)
     * @param end End of requested range (exclusive)
     * @return true if the entire range is cached
     *
     * @pre start <= end; if end < start (inverted range), the condition
     *      `start >= m_cached_start && end <= m_cached_end` can evaluate to
     *      true even though no meaningful range is requested, giving a
     *      spurious "covered" result — wrong but harmless as the caller
     *      would then skip regeneration and getVerticesForRange() returns
     *      empty for an inverted range (enforcement: none) [LOW]
     * @pre start and end must be in the same time coordinate system as the
     *      cached data (series time frame, not master time frame); mixing
     *      coordinate systems produces wrong coverage decisions
     *      (enforcement: none) [IMPORTANT]
     *
     * @post Returns false when !isValid() or vertices are empty
     * @post Returns true only when start >= cachedStart() && end <= cachedEnd()
     */
    [[nodiscard]] bool covers(TimeFrameIndex start, TimeFrameIndex end) const;

    /**
     * @brief Check if the cache needs updating for a new time range
     *
     * Returns true if:
     * - Cache is invalid or empty
     * - Requested range has no overlap with the cached window (jumped location)
     * - Requested range extends beyond the cached window at either end
     *
     * Returns false (no update needed) only when the cache is valid and
     * fully covers [@p start, @p end).
     *
     * This function is purely computational: no memory allocation, no
     * dereferences, safe to call regardless of cache validity.
     *
     * @param start Start of requested range (inclusive)
     * @param end End of requested range (exclusive)
     * @return true if vertex generation is required before rendering
     *
     * @pre start <= end; if end < start, the overlap check
     *      `end <= m_cached_start` may fire incorrectly, causing the
     *      function to return true (spurious cache miss) even when data is
     *      fully cached — wasted work but no crash or data corruption
     *      (enforcement: none) [LOW]
     * @pre start and end must be in the same time coordinate system as the
     *      cached data (series time frame, not master time frame); mixing
     *      coordinate systems produces wrong update decisions
     *      (enforcement: none) [IMPORTANT]
     *
     * @post Return value is true when !isValid() or vertices are empty
     * @post Return value is false only when covers(start, end) would return true
     */
    [[nodiscard]] bool needsUpdate(TimeFrameIndex start, TimeFrameIndex end) const;

    /**
     * @brief Calculate which ranges need to be generated
     *
     * Returns 0, 1, or 2 `MissingRange` entries describing the subranges of
     * [@p start, @p end) that are not present in the cache:
     *
     * - **0 ranges** — fully cached; nothing to generate
     * - **1 range, prepend=false** — complete cache miss (cache invalid/empty
     *   or no overlap); the single entry spans the entire requested window
     * - **1 range, prepend=true** — left edge missing (scrolling left)
     * - **1 range, prepend=false** — right edge missing (scrolling right)
     * - **2 ranges** — both edges missing; first has prepend=true,
     *   second has prepend=false
     *
     * This function is purely computational (no memory allocation beyond the
     * returned vector) and safe to call regardless of cache validity.
     *
     * @param start Start of requested range (inclusive)
     * @param end End of requested range (exclusive)
     * @return Vector of at most 2 missing sub-ranges
     *
     * @pre start <= end; if end < start, the no-overlap branch may fire and
     *      return an inverted MissingRange (start > end) that will corrupt
     *      any downstream call to generateVerticesForRange, prependVertices,
     *      or appendVertices (enforcement: none) [IMPORTANT]
     * @pre start and end must be expressed in the same time coordinate system
     *      as the indices stored in the cache (i.e., the series time frame,
     *      not the master time frame); mixing coordinate systems produces
     *      geometrically wrong gap ranges (enforcement: none) [IMPORTANT]
     *
     * @post Return value contains at most 2 entries
     * @post If return value has 2 entries, [0].prepend == true and
     *       [1].prepend == false
     * @post Every returned MissingRange satisfies start <= end when the
     *       input satisfies start <= end
     */
    [[nodiscard]] std::vector<MissingRange> getMissingRanges(TimeFrameIndex start, TimeFrameIndex end) const;

    /**
     * @brief Add vertices to the front of the cache (for scrolling left)
     *
     * Inserts @p vertices before all currently cached vertices. Vertices are
     * inserted via @c push_front in reverse so that the buffer remains sorted
     * by ascending @c time_idx. If the buffer overflows, the newest vertices
     * at the back are discarded and @c cachedEnd() is narrowed accordingly.
     *
     * If the cache is currently invalid (empty), this call initialises it
     * using the extent of @p vertices alone.
     *
     * @param vertices Vertices to prepend (may be empty; call is a no-op if so)
     * @param requested_start Start time index of the requested range (inclusive)
     *
     * @pre vertices must be sorted in ascending order by time_idx
     *      (enforcement: none) [IMPORTANT]
     * @pre If isValid() is true, every element of vertices must have
     *      time_idx < cachedStart(); otherwise the buffer becomes unsorted
     *      at the join point and binary search in getVerticesForRange()
     *      produces wrong results (enforcement: none) [IMPORTANT]
     * @pre capacity() > 0 when vertices is non-empty; if capacity is 0,
     *      push_front is a no-op yet m_valid is set to true, leaving the
     *      cache in an inconsistent state (enforcement: none) [IMPORTANT]
     *
     * @post If vertices is empty, state is unchanged
     * @post If vertices is non-empty and capacity() > 0, isValid() == true
     * @post cachedStart() <= old cachedStart() (window expands or stays)
     * @post size() <= capacity()
     */
    void prependVertices(std::vector<CachedAnalogVertex> const & vertices, TimeFrameIndex requested_start);

    /**
     * @brief Add vertices to the back of the cache (for scrolling right)
     *
     * Inserts @p vertices after all currently cached vertices via @c push_back.
     * The circular buffer automatically discards the oldest (front) entries on
     * overflow and @c cachedStart() is advanced to match.
     *
     * If the cache is currently invalid (empty), this call initialises it
     * using the extent of @p vertices alone.
     *
     * @param vertices Vertices to append (may be empty; call is a no-op if so)
     * @param requested_end End time index of the requested range (exclusive)
     *
     * @pre vertices must be sorted in ascending order by time_idx
     *      (enforcement: none) [IMPORTANT]
     * @pre If isValid() is true, every element of vertices must have
     *      time_idx >= cachedEnd(); otherwise the buffer becomes unsorted
     *      at the join point and binary search in getVerticesForRange()
     *      produces wrong results (enforcement: none) [IMPORTANT]
     * @pre capacity() > 0 when vertices is non-empty; if capacity is 0,
     *      push_back is a no-op yet m_valid is set to true and m_cached_end
     *      is overwritten with a value derived from vertices, leaving the
     *      cache in an inconsistent state (enforcement: none) [IMPORTANT]
     *
     * @post If vertices is empty, state is unchanged
     * @post If vertices is non-empty and capacity() > 0, isValid() == true
     * @post cachedEnd() >= old cachedEnd() (window expands or stays)
     * @post size() <= capacity()
     */
    void appendVertices(std::vector<CachedAnalogVertex> const & vertices, TimeFrameIndex requested_end);

    /**
     * @brief Replace all cached vertices (for cache misses or initialization)
     *
     * Clears the cache and loads @p vertices. If the circular buffer capacity
     * is smaller than @p vertices.size(), the oldest (front) entries are
     * silently discarded and the cached range is narrowed to match what was
     * actually stored.
     *
     * @param vertices New vertex data (may be empty, in which case the cache
     *        is invalidated)
     * @param start Start time index of the data (inclusive)
     * @param end End time index of the data (exclusive)
     *
     * @pre vertices must be sorted in ascending order by time_idx
     *      (enforcement: none) [IMPORTANT]
     * @pre start <= end (enforcement: none) [IMPORTANT]
     * @pre If vertices is non-empty, vertices.front().time_idx >= start
     *      (enforcement: none) [LOW]
     * @pre If vertices is non-empty, vertices.back().time_idx < end
     *      (enforcement: none) [LOW]
     * @pre capacity() > 0 when vertices is non-empty, otherwise all data is
     *      silently discarded (enforcement: runtime_check)
     *
     * @post If vertices is empty or capacity is 0, isValid() == false
     * @post If vertices is non-empty and capacity > 0, isValid() == true
     * @post size() <= capacity()
     * @post cachedStart() <= cachedEnd()
     */
    void setVertices(std::vector<CachedAnalogVertex> const & vertices,
                     TimeFrameIndex start, TimeFrameIndex end);

    /**
     * @brief Extract vertices for a specific time range
     *
     * Returns a flat float array suitable for GPU upload: [x0, y0, x1, y1, ...]
     * Uses binary search to locate @p start in the buffer, then linearly
     * scans forward until a vertex with @c time_idx >= @p end is encountered.
     *
     * Returns an empty vector (without crashing) when:
     * - The cache is invalid or empty
     * - @p start is not found in the buffer (beyond the cached range)
     *
     * @param start Start of range to extract (inclusive)
     * @param end End of range to extract (exclusive)
     * @return Flat vertex array [x0,y0,x1,y1,...], or empty if range not cached
     *
     * @pre start <= end; if end < start, the reserve() call computes
     *      static_cast<size_t>(negative_int64) * 2, which wraps to near
     *      SIZE_MAX and causes std::length_error or std::bad_alloc
     *      (enforcement: none) [CRITICAL]
     * @pre The buffer must be sorted in ascending order by time_idx;
     *      the binary search in findIndexForTime() produces wrong results
     *      if the buffer is unsorted (enforcement: none) [IMPORTANT]
     * @pre start >= cachedStart() and end <= cachedEnd() for a non-empty
     *      result; requesting a range outside the cached window returns empty
     *      (enforcement: runtime_check)
     *
     * @post Return value size is even (pairs of x,y floats)
     * @post Return value is empty when cache is invalid or start is not found
     */
    [[nodiscard]] std::vector<float> getVerticesForRange(TimeFrameIndex start, TimeFrameIndex end, TimeFrameIndex x_origin = TimeFrameIndex{0}) const;

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

}// namespace DataViewer

#endif// DATAVIEWER_WIDGET_ANALOGVERTEXCACHE_HPP
