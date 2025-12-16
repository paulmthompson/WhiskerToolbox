#ifndef COREPLOTTING_MAPPERS_MAPPEDLINEVIEW_HPP
#define COREPLOTTING_MAPPERS_MAPPEDLINEVIEW_HPP

#include "MappedElement.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <concepts>
#include <functional>
#include <ranges>
#include <span>
#include <vector>

namespace CorePlotting {

// ============================================================================
// MappedLineView - Lazy view over a single polyline
// ============================================================================

/**
 * @brief Lazy view over a mapped polyline
 * 
 * Provides zero-copy iteration over line vertices with layout transforms
 * applied on-the-fly. This enables single-traversal rendering and spatial
 * indexing without materializing intermediate vertex buffers.
 * 
 * @tparam VertexRange Type of the underlying vertex range (satisfies MappedVertexRange)
 * 
 * Usage:
 * ```cpp
 * // From Line2D with spatial mapping
 * auto line_view = MappedLineView{entity_id, line.begin(), line.end(), layout};
 * for (auto vertex : line_view.vertices()) {
 *     gpu_buffer.push_back(vertex.x);
 *     gpu_buffer.push_back(vertex.y);
 * }
 * ```
 */
template<std::ranges::input_range VertexRange>
class MappedLineView {
public:
    EntityId entity_id;  ///< Entity identifier for the entire line

    /**
     * @brief Construct a line view from an existing vertex range
     * @param id Entity identifier for this line
     * @param vertex_range Pre-computed or lazily-transformed vertex range
     */
    MappedLineView(EntityId id, VertexRange vertex_range)
        : entity_id(id)
        , _vertex_range(std::move(vertex_range)) {}

    /**
     * @brief Get the vertex range for iteration
     * @return Range of MappedVertex (or MappedVertexLike) elements
     */
    [[nodiscard]] auto vertices() const {
        return _vertex_range;
    }

    /**
     * @brief Get the vertex range for iteration (non-const)
     */
    [[nodiscard]] auto vertices() {
        return _vertex_range;
    }

private:
    VertexRange _vertex_range;
};

// ============================================================================
// SpanLineView - Simple non-owning view over pre-materialized vertices
// ============================================================================

/**
 * @brief Non-owning view over a contiguous array of MappedVertex
 * 
 * Useful when vertices have already been computed and stored,
 * providing a lightweight view interface over existing data.
 */
class SpanLineView {
public:
    EntityId entity_id;  ///< Entity identifier for the entire line

    /**
     * @brief Construct from span of vertices
     * @param id Entity identifier
     * @param verts Span over pre-computed vertices
     */
    SpanLineView(EntityId id, std::span<MappedVertex const> verts)
        : entity_id(id)
        , _vertices(verts) {}

    /**
     * @brief Construct from vector of vertices
     * @param id Entity identifier
     * @param verts Reference to vertex vector (must outlive this view)
     */
    SpanLineView(EntityId id, std::vector<MappedVertex> const & verts)
        : entity_id(id)
        , _vertices(verts) {}

    /**
     * @brief Get vertex range for iteration
     */
    [[nodiscard]] std::span<MappedVertex const> vertices() const {
        return _vertices;
    }

private:
    std::span<MappedVertex const> _vertices;
};

// ============================================================================
// OwningLineView - View that owns its vertex data
// ============================================================================

/**
 * @brief Line view that owns its vertex data
 * 
 * Useful when transformations require materialization of vertices,
 * or when the source data lifetime is uncertain.
 */
class OwningLineView {
public:
    EntityId entity_id;  ///< Entity identifier for the entire line

    /**
     * @brief Construct with ownership of vertex data
     * @param id Entity identifier
     * @param verts Vertices (moved into ownership)
     */
    OwningLineView(EntityId id, std::vector<MappedVertex> verts)
        : entity_id(id)
        , _vertices(std::move(verts)) {}

    /**
     * @brief Get vertex range for iteration
     */
    [[nodiscard]] std::span<MappedVertex const> vertices() const {
        return _vertices;
    }

    /**
     * @brief Get mutable access to vertices
     */
    [[nodiscard]] std::vector<MappedVertex> & verticesMut() {
        return _vertices;
    }

private:
    std::vector<MappedVertex> _vertices;
};

// ============================================================================
// TransformingLineView - Applies transform lazily during iteration
// ============================================================================

/**
 * @brief Line view that applies a transformation function lazily
 * 
 * Enables zero-copy coordinate transformation by wrapping an iterator
 * range and applying a transform function on dereference.
 * 
 * @tparam SourceIter Iterator type for source data
 * @tparam Transform Transform function type: SourceValue -> MappedVertex
 */
template<std::input_iterator SourceIter, typename Transform>
class TransformingLineView {
public:
    EntityId entity_id;

    TransformingLineView(EntityId id, SourceIter begin, SourceIter end, Transform transform)
        : entity_id(id)
        , _begin(begin)
        , _end(end)
        , _transform(std::move(transform)) {}

    /**
     * @brief Get a transforming view over the vertices
     */
    [[nodiscard]] auto vertices() const {
        return std::ranges::subrange(_begin, _end) 
            | std::views::transform(_transform);
    }

private:
    SourceIter _begin;
    SourceIter _end;
    Transform _transform;
};

// ============================================================================
// Factory functions for creating line views
// ============================================================================

/**
 * @brief Create a line view from a contiguous range of Point2D
 * 
 * Applies spatial layout transform (scale + offset) lazily.
 * 
 * @param id Entity identifier
 * @param points Range of Point2D<float>
 * @param x_scale X coordinate scale factor
 * @param y_scale Y coordinate scale factor  
 * @param x_offset X coordinate offset (applied after scaling)
 * @param y_offset Y coordinate offset (applied after scaling)
 */
template<std::ranges::input_range R>
requires requires(std::ranges::range_value_t<R> pt) {
    { pt.x } -> std::convertible_to<float>;
    { pt.y } -> std::convertible_to<float>;
}
[[nodiscard]] auto makeLineView(
    EntityId id,
    R const & points,
    float x_scale = 1.0f,
    float y_scale = 1.0f,
    float x_offset = 0.0f,
    float y_offset = 0.0f
) {
    auto transform = [=](auto const & pt) {
        return MappedVertex{
            pt.x * x_scale + x_offset,
            pt.y * y_scale + y_offset
        };
    };
    
    auto vertex_range = points | std::views::transform(transform);
    return MappedLineView<decltype(vertex_range)>{id, std::move(vertex_range)};
}

/**
 * @brief Create a line view with time-to-X mapping for analog series
 * 
 * @param id Entity identifier
 * @param time_value_pairs Range of (TimeFrameIndex, float) pairs
 * @param time_frame TimeFrame for index-to-time conversion
 * @param y_scale Y coordinate scale factor
 * @param y_offset Y coordinate offset
 */
template<std::ranges::input_range R>
requires requires(std::ranges::range_value_t<R> pair) {
    { std::get<0>(pair) } -> std::convertible_to<TimeFrameIndex>;
    { std::get<1>(pair) } -> std::convertible_to<float>;
}
[[nodiscard]] auto makeTimeSeriesLineView(
    EntityId id,
    R const & time_value_pairs,
    TimeFrame const & time_frame,
    float y_scale = 1.0f,
    float y_offset = 0.0f
) {
    auto transform = [&time_frame, y_scale, y_offset](auto const & pair) {
        auto const & [idx, value] = pair;
        float x = static_cast<float>(time_frame.getTimeAtIndex(idx));
        float y = value * y_scale + y_offset;
        return MappedVertex{x, y};
    };
    
    auto vertex_range = time_value_pairs | std::views::transform(transform);
    return MappedLineView<decltype(vertex_range)>{id, std::move(vertex_range)};
}

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_MAPPEDLINEVIEW_HPP
