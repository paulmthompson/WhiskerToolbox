#ifndef QUADTREE_HPP
#define QUADTREE_HPP

#include <functional>
#include <memory>
#include <vector>

/**
 * @brief A 2D point with associated data
 */
template<typename T>
struct QuadTreePoint {
    float x, y;
    T data;

    QuadTreePoint(float x, float y, T data)
        : x(x),
          y(y),
          data(std::move(data)) {}
};

/**
 * @brief Axis-aligned bounding box for spatial queries
 */
struct BoundingBox {
    float min_x, min_y, max_x, max_y;

    BoundingBox(float min_x, float min_y, float max_x, float max_y)
        : min_x(min_x),
          min_y(min_y),
          max_x(max_x),
          max_y(max_y) {}

    bool contains(float x, float y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }

    bool intersects(BoundingBox const & other) const {
        return !(other.min_x > max_x || other.max_x < min_x ||
                 other.min_y > max_y || other.max_y < min_y);
    }

    float width() const { return max_x - min_x; }
    float height() const { return max_y - min_y; }
    float center_x() const { return (min_x + max_x) * 0.5f; }
    float center_y() const { return (min_y + max_y) * 0.5f; }
};

/**
 * @brief QuadTree for efficient 2D spatial indexing and querying
 * 
 * This implementation supports inserting points with associated data and
 * efficiently querying points within a bounding box or near a specific location.
 */
template<typename T>
class QuadTree {
public:
    static constexpr int MAX_DEPTH = 8;
    static constexpr int MAX_POINTS_PER_NODE = 16;

    explicit QuadTree(BoundingBox const & bounds, int depth = 0);
    ~QuadTree() = default;

    /**
     * @brief Insert a point with associated data into the quadtree
     * @param x X coordinate
     * @param y Y coordinate
     * @param data Associated data
     * @return True if successfully inserted, false otherwise
     */
    bool insert(float x, float y, T data);

    /**
     * @brief Query points within a bounding box
     * @param query_bounds The bounding box to search within
     * @param results Vector to store found points
     */
    void query(BoundingBox const & query_bounds, std::vector<QuadTreePoint<T>> & results) const;

    /**
     * @brief Find the nearest point to the given coordinates within a maximum distance
     * @param x X coordinate to search near
     * @param y Y coordinate to search near
     * @param max_distance Maximum distance to search
     * @return Pointer to the nearest point, or nullptr if none found
     */
    QuadTreePoint<T> const * findNearest(float x, float y, float max_distance) const;

    /**
     * @brief Clear all points from the quadtree
     */
    void clear();

    /**
     * @brief Get the total number of points in the quadtree
     * @return Total point count
     */
    size_t size() const;

    /**
     * @brief Get the bounding box of this node
     * @return The bounding box
     */
    BoundingBox const & getBounds() const { return _bounds; }

private:
    BoundingBox _bounds;
    int _depth;
    std::vector<QuadTreePoint<T>> _points;
    std::unique_ptr<QuadTree> _children[4];// NW, NE, SW, SE

    /**
     * @brief Check if this node is a leaf (has no children)
     */
    bool isLeaf() const;

    /**
     * @brief Subdivide this node into four children
     */
    void subdivide();

    /**
     * @brief Get the quadrant index for a point
     * @param x X coordinate
     * @param y Y coordinate
     * @return Quadrant index (0=NW, 1=NE, 2=SW, 3=SE)
     */
    int getQuadrant(float x, float y) const;

    /**
     * @brief Calculate squared distance between two points
     */
    float distanceSquared(float x1, float y1, float x2, float y2) const;
};

// Template implementation
template<typename T>
QuadTree<T>::QuadTree(BoundingBox const & bounds, int depth)
    : _bounds(bounds),
      _depth(depth) {
    _points.reserve(MAX_POINTS_PER_NODE);
}

template<typename T>
bool QuadTree<T>::insert(float x, float y, T data) {
    if (!_bounds.contains(x, y)) {
        return false;
    }

    if (isLeaf() && (_points.size() < MAX_POINTS_PER_NODE || _depth >= MAX_DEPTH)) {
        _points.emplace_back(x, y, std::move(data));
        return true;
    }

    if (isLeaf()) {
        subdivide();
    }

    int quadrant = getQuadrant(x, y);
    return _children[quadrant]->insert(x, y, std::move(data));
}

template<typename T>
void QuadTree<T>::query(BoundingBox const & query_bounds, std::vector<QuadTreePoint<T>> & results) const {
    if (!_bounds.intersects(query_bounds)) {
        return;
    }

    for (auto const & point: _points) {
        if (query_bounds.contains(point.x, point.y)) {
            results.push_back(point);
        }
    }

    if (!isLeaf()) {
        for (int i = 0; i < 4; ++i) {
            _children[i]->query(query_bounds, results);
        }
    }
}

template<typename T>
QuadTreePoint<T> const * QuadTree<T>::findNearest(float x, float y, float max_distance) const {
    BoundingBox search_bounds(x - max_distance, y - max_distance,
                              x + max_distance, y + max_distance);

    std::vector<QuadTreePoint<T>> candidates;
    query(search_bounds, candidates);

    QuadTreePoint<T> const * nearest = nullptr;
    float min_distance_sq = max_distance * max_distance;

    for (auto const & point: candidates) {
        float dist_sq = distanceSquared(x, y, point.x, point.y);
        if (dist_sq < min_distance_sq) {
            min_distance_sq = dist_sq;
            nearest = &point;
        }
    }

    return nearest;
}

template<typename T>
void QuadTree<T>::clear() {
    _points.clear();
    for (int i = 0; i < 4; ++i) {
        _children[i].reset();
    }
}

template<typename T>
size_t QuadTree<T>::size() const {
    size_t count = _points.size();
    if (!isLeaf()) {
        for (int i = 0; i < 4; ++i) {
            count += _children[i]->size();
        }
    }
    return count;
}

template<typename T>
bool QuadTree<T>::isLeaf() const {
    return _children[0] == nullptr;
}

template<typename T>
void QuadTree<T>::subdivide() {
    float center_x = _bounds.center_x();
    float center_y = _bounds.center_y();

    // Create four children: NW, NE, SW, SE
    _children[0] = std::make_unique<QuadTree>(BoundingBox(_bounds.min_x, center_y, center_x, _bounds.max_y), _depth + 1);
    _children[1] = std::make_unique<QuadTree>(BoundingBox(center_x, center_y, _bounds.max_x, _bounds.max_y), _depth + 1);
    _children[2] = std::make_unique<QuadTree>(BoundingBox(_bounds.min_x, _bounds.min_y, center_x, center_y), _depth + 1);
    _children[3] = std::make_unique<QuadTree>(BoundingBox(center_x, _bounds.min_y, _bounds.max_x, center_y), _depth + 1);

    // Redistribute points to children
    auto points_copy = std::move(_points);
    _points.clear();

    for (auto & point: points_copy) {
        int quadrant = getQuadrant(point.x, point.y);
        _children[quadrant]->insert(point.x, point.y, std::move(point.data));
    }
}

template<typename T>
int QuadTree<T>::getQuadrant(float x, float y) const {
    float center_x = _bounds.center_x();
    float center_y = _bounds.center_y();

    if (x < center_x) {
        return (y < center_y) ? 2 : 0;// SW : NW
    } else {
        return (y < center_y) ? 3 : 1;// SE : NE
    }
}

template<typename T>
float QuadTree<T>::distanceSquared(float x1, float y1, float x2, float y2) const {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;
}

#endif// QUADTREE_HPP