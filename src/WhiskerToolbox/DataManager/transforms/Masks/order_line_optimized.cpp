#include "order_line_optimized.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <queue>

// Simple k-d tree implementation for 2D points
class KDTree {
private:
    struct Node {
        Point2D<float> point;
        std::size_t index;  // Original index of the point
        Node* left = nullptr;
        Node* right = nullptr;
        
        Node(Point2D<float> p, std::size_t idx) : point(p), index(idx) {}
        ~Node() {
            delete left;
            delete right;
        }
    };
    
    Node* root = nullptr;
    std::vector<Point2D<float>> points;
    std::vector<std::size_t> indices;
    
    // Build k-d tree recursively
    Node* buildTree(std::size_t start, std::size_t end, int depth) {
        if (start >= end) return nullptr;
        
        int axis = depth % 2;  // 0 for x, 1 for y
        
        // Sort points by the current axis
        std::size_t mid = (start + end) / 2;
        if (axis == 0) {
            std::nth_element(
                indices.begin() + start,
                indices.begin() + mid,
                indices.begin() + end,
                [this](std::size_t a, std::size_t b) {
                    return points[a].x < points[b].x;
                }
            );
        } else {
            std::nth_element(
                indices.begin() + start,
                indices.begin() + mid,
                indices.begin() + end,
                [this](std::size_t a, std::size_t b) {
                    return points[a].y < points[b].y;
                }
            );
        }
        
        Node* node = new Node(points[indices[mid]], indices[mid]);
        node->left = buildTree(start, mid, depth + 1);
        node->right = buildTree(mid + 1, end, depth + 1);
        return node;
    }
    
    // Find nearest neighbor using k-d tree
    void findNearestNeighbor(Node* node, Point2D<float> const& target, int depth,
                          std::size_t& bestIndex, float& bestDist) const {
        if (!node) return;
        
        std::size_t nodeIdx = node->index;
        Point2D<float> const& nodePoint = points[nodeIdx];
        
        // Calculate distance to current node
        float dist = std::pow(nodePoint.x - target.x, 2) + std::pow(nodePoint.y - target.y, 2);
        
        // Update best if this node is closer
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = nodeIdx;
        }
        
        // Decide which child to check first based on the splitting axis
        int axis = depth % 2;
        Node *firstChild, *secondChild;
        float axisDistance;
        
        if ((axis == 0 && target.x < nodePoint.x) || 
            (axis == 1 && target.y < nodePoint.y)) {
            firstChild = node->left;
            secondChild = node->right;
        } else {
            firstChild = node->right;
            secondChild = node->left;
        }
        
        // Check first child
        findNearestNeighbor(firstChild, target, depth + 1, bestIndex, bestDist);
        
        // Calculate distance to the splitting plane
        if (axis == 0) {
            axisDistance = std::pow(nodePoint.x - target.x, 2);
        } else {
            axisDistance = std::pow(nodePoint.y - target.y, 2);
        }
        
        // If the distance to the splitting plane is less than the current best distance,
        // the other side could contain a closer point
        if (axisDistance < bestDist) {
            findNearestNeighbor(secondChild, target, depth + 1, bestIndex, bestDist);
        }
    }
    
public:
    KDTree(std::vector<Point2D<float>> const& pts) : points(pts) {
        indices.resize(points.size());
        for (std::size_t i = 0; i < points.size(); ++i) {
            indices[i] = i;
        }
        
        if (!points.empty()) {
            root = buildTree(0, points.size(), 0);
        }
    }
    
    ~KDTree() {
        delete root;
    }
    
    // Find nearest neighbor to the target point
    std::size_t findNearest(Point2D<float> const& target) const {
        if (points.empty()) return std::numeric_limits<std::size_t>::max();
        
        std::size_t bestIndex = 0;
        float bestDist = std::numeric_limits<float>::max();
        
        findNearestNeighbor(root, target, 0, bestIndex, bestDist);
        return bestIndex;
    }
};

std::vector<Point2D<float>> order_line_optimized(
        std::vector<uint8_t> const & binary_img,
        ImageSize const image_size,
        Point2D<float> const & origin,
        int subsample,
        float tolerance) {

    auto const height = image_size.height;
    auto const width = image_size.width;
    
    // Extract coordinates of the line pixels
    std::vector<Point2D<float>> line_pixels;
    line_pixels.reserve(width * height / 10); // Reserve some space to avoid frequent reallocations
    
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (binary_img[row * width + col] == 1) {
                line_pixels.push_back({static_cast<float>(col), static_cast<float>(row)});
            }
        }
    }

    // If there are no line pixels, return empty vector
    if (line_pixels.empty()) {
        return {};
    }

    // Subsample the line pixels if subsample > 1
    if (subsample > 1) {
        std::vector<Point2D<float>> subsampled_pixels;
        subsampled_pixels.reserve(line_pixels.size() / subsample + 1);
        
        for (size_t i = 0; i < line_pixels.size(); i += subsample) {
            subsampled_pixels.push_back(line_pixels[i]);
        }
        line_pixels = std::move(subsampled_pixels);
    }
    
    // Find base point (closest to origin)
    float min_dist = std::numeric_limits<float>::max();
    std::size_t base_idx = 0;
    
    for (std::size_t i = 0; i < line_pixels.size(); ++i) {
        float dist = std::pow(line_pixels[i].x - origin.x, 2) + std::pow(line_pixels[i].y - origin.y, 2);
        if (dist < min_dist) {
            min_dist = dist;
            base_idx = i;
        }
    }
    
    // Initialize ordered points with the base point
    std::vector<Point2D<float>> ordered_points;
    ordered_points.reserve(line_pixels.size());
    ordered_points.push_back(line_pixels[base_idx]);
    
    // Create a vector of flags to mark visited points
    std::vector<bool> visited(line_pixels.size(), false);
    visited[base_idx] = true;
    
    // Create a k-d tree for efficient nearest neighbor search
    KDTree kdtree(line_pixels);
    
    // Keep track of distances for applying tolerance later
    std::vector<float> distances;
    distances.reserve(line_pixels.size() - 1);
    
    // Current point
    Point2D<float> current = line_pixels[base_idx];
    std::size_t points_remaining = line_pixels.size() - 1;
    
    // Find nearest neighbors iteratively
    while (points_remaining > 0) {
        // Find the nearest unvisited neighbor
        std::size_t nearest_idx = 0;
        float nearest_dist = std::numeric_limits<float>::max();
        
        // Optimization: Instead of searching through all points each time,
        // we could use a priority queue or a more sophisticated data structure.
        // But for simplicity, we'll use a linear search here.
        for (std::size_t i = 0; i < line_pixels.size(); ++i) {
            if (!visited[i]) {
                float dist = std::pow(line_pixels[i].x - current.x, 2) + std::pow(line_pixels[i].y - current.y, 2);
                if (dist < nearest_dist) {
                    nearest_dist = dist;
                    nearest_idx = i;
                }
            }
        }
        
        // Add the nearest point to the ordered list
        ordered_points.push_back(line_pixels[nearest_idx]);
        distances.push_back(nearest_dist);
        
        // Mark as visited and update current point
        visited[nearest_idx] = true;
        current = line_pixels[nearest_idx];
        points_remaining--;
    }
    
    // Apply tolerance filtering
    if (tolerance > 0.0f) {
        std::vector<Point2D<float>> filtered_points;
        filtered_points.reserve(ordered_points.size());
        
        // Always include the first point
        filtered_points.push_back(ordered_points[0]);
        
        // Filter subsequent points based on distance
        for (std::size_t i = 1; i < ordered_points.size(); ++i) {
            if (distances[i - 1] <= tolerance * tolerance) { // Square the tolerance for comparison
                filtered_points.push_back(ordered_points[i]);
            }
        }
        
        return filtered_points;
    }
    
    return ordered_points;
} 