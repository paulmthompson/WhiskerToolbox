#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "KdTree.hpp"
#include "CoreGeometry/points.hpp"

#include <vector>

TEST_CASE("KD-Tree Construction", "[kdtree]") {
    std::vector<Kdtree::KdNode<float>> nodes;
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(1.0f, 2.0f)));
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(3.0f, 4.0f)));
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(5.0f, 6.0f)));

    Kdtree::KdTree<float> tree(&nodes);

    REQUIRE(tree.allnodes.size() == 3);
}

TEST_CASE("KD-Tree Nearest Neighbor", "[kdtree]") {
    std::vector<Kdtree::KdNode<float>> nodes;
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(1.0f, 2.0f)));
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(3.0f, 4.0f)));
    nodes.push_back(Kdtree::KdNode<float>(Point2D<float>(5.0f, 6.0f)));

    Kdtree::KdTree<float> tree(&nodes);

    Point2D<float> query_point(1.1f, 2.1f);
    std::vector<Kdtree::KdNode<float>> result;
    tree.k_nearest_neighbors(query_point, 1, &result);

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].point.x == Catch::Approx(1.0f));
    REQUIRE(result[0].point.y == Catch::Approx(2.0f));
}
