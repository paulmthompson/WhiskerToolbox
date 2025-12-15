#ifndef COREPLOTTING_SPATIALADAPTER_POLYLINESPATIALADAPTER_HPP
#define COREPLOTTING_SPATIALADAPTER_POLYLINESPATIALADAPTER

#include "SceneGraph/RenderablePrimitives.hpp"

#include "Entity/EntityTypes.hpp"
#include "SpatialIndex/QuadTree.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace CorePlotting {

/**
 * @brief Builds spatial indexes from polyline geometry
 * 
 * Creates QuadTree<EntityId> for spatial queries on line segments.
 * Handles both vertex-level indexing and bounding box strategies.
 */
class PolyLineSpatialAdapter {
public:
    /**
     * @brief Build spatial index from polyline batch using vertex sampling
     * 
     * Inserts every vertex of every line into the spatial index.
     * Best for sparse data or when precise vertex selection is needed.
     * 
     * @param batch RenderablePolyLineBatch containing line geometry
     * @param bounds BoundingBox for the QuadTree
     * @return QuadTree with (x,y) = vertex positions, data = EntityId
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromVertices(
            RenderablePolyLineBatch const & batch,
            BoundingBox const & bounds);

    /**
     * @brief Build spatial index from polyline batch using bounding boxes
     * 
     * Computes axis-aligned bounding box for each line and inserts corner points.
     * More efficient for dense data or when line-level selection is sufficient.
     * 
     * @param batch RenderablePolyLineBatch containing line geometry
     * @param bounds BoundingBox for the QuadTree
     * @return QuadTree with (x,y) = AABB corners, data = EntityId
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromBoundingBoxes(
            RenderablePolyLineBatch const & batch,
            BoundingBox const & bounds);

    /**
     * @brief Build spatial index from polyline batch using uniform sampling
     * 
     * Samples points along each line at regular intervals.
     * Balances precision and performance for very long polylines.
     * 
     * @param batch RenderablePolyLineBatch containing line geometry
     * @param sample_interval Spacing between sampled points (in world units)
     * @param bounds BoundingBox for the QuadTree
     * @return QuadTree with (x,y) = sampled positions, data = EntityId
     */
    static std::unique_ptr<QuadTree<EntityId>> buildFromSampledPoints(
            RenderablePolyLineBatch const & batch,
            float sample_interval,
            BoundingBox const & bounds);
};

}// namespace CorePlotting

#endif// COREPLOTTING_SPATIALADAPTER_POLYLINESPATIALADAPTER_HPP