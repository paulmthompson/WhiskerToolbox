#ifndef RTREE_HPP
#define RTREE_HPP

#include "CoreGeometry/boundingbox.hpp"

#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include <algorithm>
#include <limits>

/**
 * @brief A bounding box with associated data for R-tree storage
 */
template<typename T>
struct RTreeEntry {
    float min_x, min_y, max_x, max_y;
    T data;

    // Default constructor
    RTreeEntry() = default;

    RTreeEntry(float min_x, float min_y, float max_x, float max_y, T data)
        : min_x(min_x), min_y(min_y), max_x(max_x), max_y(max_y), data(std::move(data)) {}

    RTreeEntry(const BoundingBox& bbox, T data)
        : min_x(bbox.min_x), min_y(bbox.min_y), max_x(bbox.max_x), max_y(bbox.max_y), data(std::move(data)) {}

    bool contains(float x, float y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }

    bool intersects(const RTreeEntry& other) const {
        return !(other.min_x > max_x || other.max_x < min_x ||
                 other.min_y > max_y || other.max_y < min_y);
    }

    bool intersects(const BoundingBox& bbox) const {
        return !(bbox.min_x > max_x || bbox.max_x < min_x ||
                 bbox.min_y > max_y || bbox.max_y < min_y);
    }

    float area() const {
        return (max_x - min_x) * (max_y - min_y);
    }

    float width() const { return max_x - min_x; }
    float height() const { return max_y - min_y; }
    float center_x() const { return (min_x + max_x) * 0.5f; }
    float center_y() const { return (min_y + max_y) * 0.5f; }

    BoundingBox toBoundingBox() const {
        return BoundingBox(min_x, min_y, max_x, max_y);
    }

    /**
     * @brief Calculate the minimum distance from a point to this bounding box
     * @param x X coordinate of the point
     * @param y Y coordinate of the point
     * @return Minimum distance (0 if point is inside the box)
     */
    float distanceToPoint(float x, float y) const {
        float dx = 0.0f;
        float dy = 0.0f;
        
        if (x < min_x) dx = min_x - x;
        else if (x > max_x) dx = x - max_x;
        
        if (y < min_y) dy = min_y - y;
        else if (y > max_y) dy = y - max_y;
        
        return std::sqrt(dx * dx + dy * dy);
    }

    /**
     * @brief Calculate the squared minimum distance from a point to this bounding box
     * @param x X coordinate of the point
     * @param y Y coordinate of the point
     * @return Squared minimum distance (0 if point is inside the box)
     */
    float distanceToPointSquared(float x, float y) const {
        float dx = 0.0f;
        float dy = 0.0f;
        
        if (x < min_x) dx = min_x - x;
        else if (x > max_x) dx = x - max_x;
        
        if (y < min_y) dy = min_y - y;
        else if (y > max_y) dy = y - max_y;
        
        return dx * dx + dy * dy;
    }
};

/**
 * @brief R-tree for efficient 2D spatial indexing and querying of bounding boxes
 * 
 * This implementation is optimized for storing and querying rectangular regions.
 * It supports efficient "contains point" queries and nearest neighbor searches.
 */
template<typename T>
class RTree {
private:
    struct RTreeNode {
        std::vector<RTreeEntry<T>> entries;
        std::vector<std::unique_ptr<RTreeNode>> children;
        bool is_leaf;
        
        // Bounding box that encompasses all entries in this node
        float min_x, min_y, max_x, max_y;
        
        RTreeNode(bool leaf = true) : is_leaf(leaf) {
            min_x = min_y = std::numeric_limits<float>::max();
            max_x = max_y = std::numeric_limits<float>::lowest();
        }
        
        void updateBounds() {
            min_x = min_y = std::numeric_limits<float>::max();
            max_x = max_y = std::numeric_limits<float>::lowest();
            
            if (is_leaf) {
                for (const auto& entry : entries) {
                    min_x = std::min(min_x, entry.min_x);
                    min_y = std::min(min_y, entry.min_y);
                    max_x = std::max(max_x, entry.max_x);
                    max_y = std::max(max_y, entry.max_y);
                }
            } else {
                for (const auto& child : children) {
                    min_x = std::min(min_x, child->min_x);
                    min_y = std::min(min_y, child->min_y);
                    max_x = std::max(max_x, child->max_x);
                    max_y = std::max(max_y, child->max_y);
                }
            }
        }
        
        bool intersects(const BoundingBox& bbox) const {
            return !(bbox.min_x > max_x || bbox.max_x < min_x ||
                     bbox.min_y > max_y || bbox.max_y < min_y);
        }
        
        bool contains(float x, float y) const {
            return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
        }
        
        float area() const {
            return (max_x - min_x) * (max_y - min_y);
        }
        
        float enlargementArea(const RTreeEntry<T>& entry) const {
            float new_min_x = std::min(min_x, entry.min_x);
            float new_min_y = std::min(min_y, entry.min_y);
            float new_max_x = std::max(max_x, entry.max_x);
            float new_max_y = std::max(max_y, entry.max_y);
            
            float new_area = (new_max_x - new_min_x) * (new_max_y - new_min_y);
            return new_area - area();
        }
    };

public:
    static constexpr int MIN_ENTRIES = 2;
    static constexpr int MAX_ENTRIES = 8;

    RTree() : root(std::make_unique<RTreeNode>(true)), size_(0) {}
    ~RTree() = default;

    // Move constructor and assignment operator
    RTree(RTree && other) noexcept : root(std::move(other.root)), size_(other.size_) {
        other.size_ = 0;
    }
    
    RTree & operator=(RTree && other) noexcept {
        if (this != &other) {
            root = std::move(other.root);
            size_ = other.size_;
            other.size_ = 0;
        }
        return *this;
    }

    // Disable copy constructor and assignment operator
    RTree(const RTree&) = delete;
    RTree & operator=(const RTree&) = delete;

    /**
     * @brief Insert a bounding box with associated data into the R-tree
     * @param bbox The bounding box to insert
     * @param data Associated data
     * @return True if successfully inserted
     */
    bool insert(const BoundingBox& bbox, T data) {
        return insert(bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y, std::move(data));
    }

    /**
     * @brief Insert a bounding box with associated data into the R-tree
     * @param min_x Minimum X coordinate
     * @param min_y Minimum Y coordinate
     * @param max_x Maximum X coordinate
     * @param max_y Maximum Y coordinate
     * @param data Associated data
     * @return True if successfully inserted
     */
    bool insert(float min_x, float min_y, float max_x, float max_y, T data) {
        if (min_x > max_x || min_y > max_y) {
            return false; // Invalid bounding box
        }
        
        RTreeEntry<T> entry(min_x, min_y, max_x, max_y, std::move(data));
        auto new_root = insertEntry(root.get(), entry);
        
        if (new_root) {
            // Root was split, create new root
            auto old_root = std::move(root);
            root = std::make_unique<RTreeNode>(false);
            root->children.push_back(std::move(old_root));
            root->children.push_back(std::move(new_root));
            root->updateBounds();
        }
        
        size_++;
        return true;
    }

    /**
     * @brief Query entries that intersect with a bounding box
     * @param query_bounds The bounding box to search within
     * @param results Vector to store found entries
     */
    void query(const BoundingBox& query_bounds, std::vector<RTreeEntry<T>>& results) const {
        if (root) {
            queryNode(root.get(), query_bounds, results);
        }
    }

    /**
     * @brief Query entries that intersect with a bounding box, returning pointers
     * @param query_bounds The bounding box to search within
     * @param results Vector to store pointers to found entries
     */
    void queryPointers(const BoundingBox& query_bounds, std::vector<const RTreeEntry<T>*>& results) const {
        if (root) {
            queryNodePointers(root.get(), query_bounds, results);
        }
    }

    /**
     * @brief Find all entries that contain the given point
     * @param x X coordinate
     * @param y Y coordinate
     * @param results Vector to store found entries
     */
    void queryPoint(float x, float y, std::vector<RTreeEntry<T>>& results) const {
        if (root) {
            queryPointNode(root.get(), x, y, results);
        }
    }

    /**
     * @brief Find all entries that contain the given point, returning pointers
     * @param x X coordinate
     * @param y Y coordinate
     * @param results Vector to store pointers to found entries
     */
    void queryPointPointers(float x, float y, std::vector<const RTreeEntry<T>*>& results) const {
        if (root) {
            queryPointNodePointers(root.get(), x, y, results);
        }
    }

    /**
     * @brief Find the nearest entry to the given point within a maximum distance
     * @param x X coordinate to search near
     * @param y Y coordinate to search near
     * @param max_distance Maximum distance to search
     * @return Pointer to the nearest entry, or nullptr if none found
     */
    const RTreeEntry<T>* findNearest(float x, float y, float max_distance) const {
        if (!root) return nullptr;
        
        const RTreeEntry<T>* nearest = nullptr;
        float min_distance_sq = max_distance * max_distance;
        
        findNearestNode(root.get(), x, y, min_distance_sq, nearest);
        return nearest;
    }

    /**
     * @brief Clear all entries from the R-tree
     */
    void clear() {
        root = std::make_unique<RTreeNode>(true);
        size_ = 0;
    }

    /**
     * @brief Get the total number of entries in the R-tree
     * @return Total entry count
     */
    size_t size() const { return size_; }

    /**
     * @brief Get the bounding box that encompasses all entries
     * @return The root bounding box
     */
    BoundingBox getBounds() const {
        if (!root || size_ == 0) {
            return BoundingBox(0, 0, 0, 0);
        }
        return BoundingBox(root->min_x, root->min_y, root->max_x, root->max_y);
    }

private:
    std::unique_ptr<RTreeNode> root;
    size_t size_;

    /**
     * @brief Insert an entry into the tree, possibly splitting nodes
     * @param node The node to insert into
     * @param entry The entry to insert
     * @return New node if split occurred, nullptr otherwise
     */
    std::unique_ptr<RTreeNode> insertEntry(RTreeNode* node, const RTreeEntry<T>& entry) {
        if (node->is_leaf) {
            // Insert into leaf node
            node->entries.push_back(entry);
            node->updateBounds();
            
            if (node->entries.size() > MAX_ENTRIES) {
                return splitNode(node);
            }
            return nullptr;
        } else {
            // Choose subtree to insert into
            RTreeNode* best_child = chooseSubtree(node, entry);
            auto new_node = insertEntry(best_child, entry);
            
            if (new_node) {
                // Child was split
                node->children.push_back(std::move(new_node));
                node->updateBounds();
                
                if (node->children.size() > MAX_ENTRIES) {
                    return splitNode(node);
                }
            } else {
                node->updateBounds();
            }
            
            return nullptr;
        }
    }

    /**
     * @brief Choose the best child node to insert an entry into
     * @param node Parent node
     * @param entry Entry to insert
     * @return Best child node
     */
    RTreeNode* chooseSubtree(RTreeNode* node, const RTreeEntry<T>& entry) {
        RTreeNode* best = nullptr;
        float min_enlargement = std::numeric_limits<float>::max();
        float min_area = std::numeric_limits<float>::max();
        
        for (const auto& child : node->children) {
            float enlargement = child->enlargementArea(entry);
            float area = child->area();
            
            if (enlargement < min_enlargement || 
                (enlargement == min_enlargement && area < min_area)) {
                min_enlargement = enlargement;
                min_area = area;
                best = child.get();
            }
        }
        
        return best;
    }

    /**
     * @brief Split a node that has exceeded maximum capacity
     * @param node Node to split
     * @return New node containing half the entries
     */
    std::unique_ptr<RTreeNode> splitNode(RTreeNode* node) {
        auto new_node = std::make_unique<RTreeNode>(node->is_leaf);
        
        if (node->is_leaf) {
            // Split leaf node entries
            auto& entries = node->entries;
            size_t split_point = entries.size() / 2;
            
            // Use linear split (simple but effective)
            std::sort(entries.begin(), entries.end(), 
                     [](const RTreeEntry<T>& a, const RTreeEntry<T>& b) {
                         return a.center_x() < b.center_x();
                     });
            
            new_node->entries.assign(entries.begin() + split_point, entries.end());
            entries.resize(split_point);
        } else {
            // Split internal node children
            auto& children = node->children;
            size_t split_point = children.size() / 2;
            
            // Sort by center x coordinate for linear split
            std::sort(children.begin(), children.end(), 
                     [](const std::unique_ptr<RTreeNode>& a, const std::unique_ptr<RTreeNode>& b) {
                         return (a->min_x + a->max_x) < (b->min_x + b->max_x);
                     });
            
            new_node->children.assign(
                std::make_move_iterator(children.begin() + split_point),
                std::make_move_iterator(children.end())
            );
            children.resize(split_point);
        }
        
        node->updateBounds();
        new_node->updateBounds();
        
        return new_node;
    }

    /**
     * @brief Query a node for intersecting entries
     */
    void queryNode(RTreeNode* node, const BoundingBox& query_bounds, 
                   std::vector<RTreeEntry<T>>& results) const {
        if (!node->intersects(query_bounds)) {
            return;
        }
        
        if (node->is_leaf) {
            for (const auto& entry : node->entries) {
                if (entry.intersects(query_bounds)) {
                    results.push_back(entry);
                }
            }
        } else {
            for (const auto& child : node->children) {
                queryNode(child.get(), query_bounds, results);
            }
        }
    }

    /**
     * @brief Query a node for intersecting entries, returning pointers
     */
    void queryNodePointers(RTreeNode* node, const BoundingBox& query_bounds, 
                          std::vector<const RTreeEntry<T>*>& results) const {
        if (!node->intersects(query_bounds)) {
            return;
        }
        
        if (node->is_leaf) {
            for (const auto& entry : node->entries) {
                if (entry.intersects(query_bounds)) {
                    results.push_back(&entry);
                }
            }
        } else {
            for (const auto& child : node->children) {
                queryNodePointers(child.get(), query_bounds, results);
            }
        }
    }

    /**
     * @brief Query a node for entries that contain a point
     */
    void queryPointNode(RTreeNode* node, float x, float y, 
                       std::vector<RTreeEntry<T>>& results) const {
        if (!node->contains(x, y)) {
            return;
        }
        
        if (node->is_leaf) {
            for (const auto& entry : node->entries) {
                if (entry.contains(x, y)) {
                    results.push_back(entry);
                }
            }
        } else {
            for (const auto& child : node->children) {
                queryPointNode(child.get(), x, y, results);
            }
        }
    }

    /**
     * @brief Query a node for entries that contain a point, returning pointers
     */
    void queryPointNodePointers(RTreeNode* node, float x, float y, 
                               std::vector<const RTreeEntry<T>*>& results) const {
        if (!node->contains(x, y)) {
            return;
        }
        
        if (node->is_leaf) {
            for (const auto& entry : node->entries) {
                if (entry.contains(x, y)) {
                    results.push_back(&entry);
                }
            }
        } else {
            for (const auto& child : node->children) {
                queryPointNodePointers(child.get(), x, y, results);
            }
        }
    }

    /**
     * @brief Find the nearest entry in a node
     */
    void findNearestNode(RTreeNode* node, float x, float y, float& min_distance_sq,
                        const RTreeEntry<T>*& nearest) const {
        if (node->is_leaf) {
            for (const auto& entry : node->entries) {
                float dist_sq = entry.distanceToPointSquared(x, y);
                if (dist_sq < min_distance_sq) {
                    min_distance_sq = dist_sq;
                    nearest = &entry;
                }
            }
        } else {
            // Create a list of children with their minimum distances, sorted by distance
            std::vector<std::pair<float, RTreeNode*>> child_distances;
            
            for (const auto& child : node->children) {
                // Calculate minimum distance to child's bounding box
                float dx = 0.0f, dy = 0.0f;
                
                if (x < child->min_x) dx = child->min_x - x;
                else if (x > child->max_x) dx = x - child->max_x;
                
                if (y < child->min_y) dy = child->min_y - y;
                else if (y > child->max_y) dy = y - child->max_y;
                
                float dist_sq = dx * dx + dy * dy;
                child_distances.emplace_back(dist_sq, child.get());
            }
            
            // Sort children by minimum distance to ensure we explore closest first
            std::sort(child_distances.begin(), child_distances.end());
            
            // Explore children in order of increasing minimum distance
            for (const auto& [dist_sq, child] : child_distances) {
                // If the minimum distance to this child is already greater than our best,
                // we can skip it and all remaining children (since they're sorted)
                if (dist_sq >= min_distance_sq) {
                    break;
                }
                
                // Recursively search this child
                findNearestNode(child, x, y, min_distance_sq, nearest);
            }
        }
    }
};

#endif // RTREE_HPP
