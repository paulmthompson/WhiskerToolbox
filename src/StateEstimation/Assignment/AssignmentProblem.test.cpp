#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Assignment/AssignmentProblem.hpp"
#include "Features/FeatureVector.hpp"

#include <Eigen/Dense>

#include <set>

using namespace StateEstimation;
using Catch::Matchers::WithinAbs;

TEST_CASE("StateEstimation - Assignment - AssignmentResult", "[Assignment]") {
    SECTION("Default construction") {
        AssignmentResult result;
        
        REQUIRE(result.total_cost == 0.0);
        REQUIRE(result.success == false);
        REQUIRE(result.assignments.empty());
        REQUIRE(result.costs.empty());
    }
    
    SECTION("Valid result") {
        AssignmentResult result;
        result.success = true;
        result.total_cost = 15.5;
        result.assignments = {1, 0, -1}; // Object 0->Target 1, Object 1->Target 0, Object 2 unassigned
        result.costs = {5.5, 10.0, std::numeric_limits<double>::infinity()};
        
        REQUIRE(result.success == true);
        REQUIRE_THAT(result.total_cost, WithinAbs(15.5, 1e-6));
        REQUIRE(result.assignments.size() == 3);
        REQUIRE(result.costs.size() == 3);
        REQUIRE(result.assignments[2] == -1); // Unassigned
    }
}

TEST_CASE("StateEstimation - Assignment - AssignmentConstraints", "[Assignment]") {
    SECTION("Default constraints") {
        AssignmentConstraints constraints;
        
        REQUIRE(constraints.max_cost == std::numeric_limits<double>::infinity());
        REQUIRE(constraints.allow_unassigned == true);
        REQUIRE(constraints.one_to_one == true);
        REQUIRE(constraints.required_features.empty());
        REQUIRE(constraints.optional_features.empty());
    }
    
    SECTION("Custom constraints") {
        AssignmentConstraints constraints;
        constraints.max_cost = 100.0;
        constraints.allow_unassigned = false;
        constraints.required_features = {"position"};
        constraints.optional_features = {"length", "orientation"};
        
        REQUIRE_THAT(constraints.max_cost, WithinAbs(100.0, 1e-6));
        REQUIRE(constraints.allow_unassigned == false);
        REQUIRE(constraints.required_features.size() == 1);
        REQUIRE(constraints.optional_features.size() == 2);
    }
}

TEST_CASE("StateEstimation - Assignment - HungarianAssignment", "[Assignment]") {
    SECTION("Construction") {
        HungarianAssignment assignment;
        REQUIRE(assignment.getName() == "Hungarian Algorithm");
    }
    
    SECTION("Construction with custom cost function") {
        auto custom_cost = [](FeatureVector const&, FeatureVector const&) { return 42.0; };
        HungarianAssignment assignment(custom_cost);
        REQUIRE(assignment.getName() == "Hungarian Algorithm");
    }
    
    SECTION("Simple 2x2 cost matrix") {
        HungarianAssignment assignment;
        
        // Cost matrix: prefer diagonal assignment (0->0, 1->1)
        std::vector<std::vector<double>> cost_matrix = {
            {1.0, 10.0},  // Object 0: cheap to assign to target 0
            {10.0, 1.0}   // Object 1: cheap to assign to target 1
        };
        
        auto result = assignment.solve(cost_matrix);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 2);
        REQUIRE_THAT(result.total_cost, WithinAbs(2.0, 1e-3)); // 1.0 + 1.0
        
        // Should assign 0->0 and 1->1
        REQUIRE((result.assignments[0] == 0 && result.assignments[1] == 1));
    }
    
    SECTION("3x3 cost matrix") {
        HungarianAssignment assignment;
        
        std::vector<std::vector<double>> cost_matrix = {
            {2.0, 4.0, 6.0},
            {3.0, 1.0, 5.0},
            {7.0, 8.0, 2.0}
        };
        
        auto result = assignment.solve(cost_matrix);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 3);
        
        // Verify all assignments are unique (one-to-one)
        std::set<int> assigned_targets;
        for (int target : result.assignments) {
            if (target >= 0) {
                REQUIRE(assigned_targets.find(target) == assigned_targets.end());
                assigned_targets.insert(target);
            }
        }
    }
    
    SECTION("Assignment with cost constraint") {
        HungarianAssignment assignment;
        
        std::vector<std::vector<double>> cost_matrix = {
            {1.0, 100.0},  // Object 0: target 1 is too expensive
            {100.0, 1.0}   // Object 1: target 0 is too expensive
        };
        
        AssignmentConstraints constraints;
        constraints.max_cost = 50.0; // Prohibit expensive assignments
        
        auto result = assignment.solve(cost_matrix, constraints);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 2);
        
        // Should assign 0->0 and 1->1 (cheap assignments)
        REQUIRE(result.assignments[0] == 0);
        REQUIRE(result.assignments[1] == 1);
        REQUIRE_THAT(result.total_cost, WithinAbs(2.0, 1e-3));
    }
    
    SECTION("Assignment with feature vectors") {
        HungarianAssignment assignment;
        
        // Create objects and targets with position features
        std::vector<FeatureVector> objects;
        std::vector<FeatureVector> targets;
        
        // Object 0 at (0, 0)
        FeatureVector obj0;
        Eigen::Vector2d pos0(0.0, 0.0);
        obj0.addFeature("position", FeatureType::POSITION, pos0);
        objects.push_back(obj0);
        
        // Object 1 at (10, 0)
        FeatureVector obj1;
        Eigen::Vector2d pos1(10.0, 0.0);
        obj1.addFeature("position", FeatureType::POSITION, pos1);
        objects.push_back(obj1);
        
        // Target 0 at (1, 0) - close to object 0
        FeatureVector target0;
        Eigen::Vector2d tpos0(1.0, 0.0);
        target0.addFeature("position", FeatureType::POSITION, tpos0);
        targets.push_back(target0);
        
        // Target 1 at (9, 0) - close to object 1
        FeatureVector target1;
        Eigen::Vector2d tpos1(9.0, 0.0);
        target1.addFeature("position", FeatureType::POSITION, tpos1);
        targets.push_back(target1);
        
        auto result = assignment.solve(objects, targets);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 2);
        
        // Should assign objects to nearest targets
        REQUIRE(result.assignments[0] == 0); // Object 0 -> Target 0
        REQUIRE(result.assignments[1] == 1); // Object 1 -> Target 1
        
        // Costs should be the euclidean distances
        REQUIRE_THAT(result.costs[0], WithinAbs(1.0, 1e-6)); // Distance from (0,0) to (1,0)
        REQUIRE_THAT(result.costs[1], WithinAbs(1.0, 1e-6)); // Distance from (10,0) to (9,0)
    }
    
    SECTION("More objects than targets") {
        HungarianAssignment assignment;
        
        std::vector<std::vector<double>> cost_matrix = {
            {1.0, 2.0},    // Object 0
            {3.0, 1.0},    // Object 1  
            {2.0, 3.0}     // Object 2
        };
        
        auto result = assignment.solve(cost_matrix);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 3);
        
        // Two objects should be assigned, one should be unassigned (-1)
        int assigned_count = 0;
        int unassigned_count = 0;
        
        for (int assignment : result.assignments) {
            if (assignment >= 0) {
                assigned_count++;
            } else {
                unassigned_count++;
            }
        }
        
        REQUIRE(assigned_count == 2);
        REQUIRE(unassigned_count == 1);
    }
    
    SECTION("Empty inputs") {
        HungarianAssignment assignment;
        
        std::vector<FeatureVector> empty_objects;
        std::vector<FeatureVector> empty_targets;
        
        auto result = assignment.solve(empty_objects, empty_targets);
        
        REQUIRE(result.success == false);
        REQUIRE(result.assignments.empty());
    }
    
    SECTION("Required features constraint") {
        HungarianAssignment assignment;
        
        // Create objects with different features
        std::vector<FeatureVector> objects;
        std::vector<FeatureVector> targets;
        
        // Object with position only
        FeatureVector obj0;
        Eigen::Vector2d pos0(0.0, 0.0);
        obj0.addFeature("position", FeatureType::POSITION, pos0);
        objects.push_back(obj0);
        
        // Object with position and length
        FeatureVector obj1;
        Eigen::Vector2d pos1(1.0, 1.0);
        Eigen::VectorXd len1(1); len1[0] = 5.0;
        obj1.addFeature("position", FeatureType::POSITION, pos1);
        obj1.addFeature("length", FeatureType::SCALE, len1);
        objects.push_back(obj1);
        
        // Target with position and length
        FeatureVector target0;
        Eigen::Vector2d tpos0(0.5, 0.5);
        Eigen::VectorXd tlen0(1); tlen0[0] = 4.0;
        target0.addFeature("position", FeatureType::POSITION, tpos0);
        target0.addFeature("length", FeatureType::SCALE, tlen0);
        targets.push_back(target0);
        
        // Require both position and length features
        AssignmentConstraints constraints;
        constraints.required_features = {"position", "length"};
        
        auto result = assignment.solve(objects, targets, constraints);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 2);
        
        // Only object 1 should be assignable (has both required features)
        REQUIRE(result.assignments[0] == -1); // Object 0 missing length
        REQUIRE(result.assignments[1] == 0);  // Object 1 has both features
    }
}

TEST_CASE("StateEstimation - Assignment - Cost Functions", "[Assignment]") {
    SECTION("Euclidean distance") {
        FeatureVector obj, target;
        
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector2d target_pos(3.0, 4.0);
        
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        double distance = CostFunctions::euclideanDistance(obj, target);
        REQUIRE_THAT(distance, WithinAbs(5.0, 1e-6)); // 3-4-5 triangle
    }
    
    SECTION("Euclidean distance with dimension mismatch") {
        FeatureVector obj, target;
        
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector3d target_pos(1.0, 2.0, 3.0);
        
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        double distance = CostFunctions::euclideanDistance(obj, target);
        REQUIRE(distance == std::numeric_limits<double>::infinity());
    }
    
    SECTION("Mahalanobis distance") {
        // Create covariance matrix (2x2 identity scaled by 4)
        Eigen::Matrix2d covariance = Eigen::Matrix2d::Identity() * 4.0;
        CostFunctions::MahalanobisDistance mahal_dist(covariance);
        
        FeatureVector obj, target;
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector2d target_pos(2.0, 0.0);
        
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        double distance = mahal_dist(obj, target);
        REQUIRE_THAT(distance, WithinAbs(1.0, 1e-6)); // sqrt((2^2) / 4) = 1
    }
    
    SECTION("Feature weighted distance") {
        std::unordered_map<std::string, double> weights;
        weights["position"] = 1.0;
        weights["length"] = 0.5;  // Half weight for length
        
        CostFunctions::FeatureWeightedDistance weighted_dist(weights);
        
        FeatureVector obj, target;
        
        // Position difference: (1,1)
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector2d target_pos(1.0, 1.0);
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        // Length difference: 2
        Eigen::VectorXd obj_len(1); obj_len[0] = 5.0;
        Eigen::VectorXd target_len(1); target_len[0] = 7.0;
        obj.addFeature("length", FeatureType::SCALE, obj_len);
        target.addFeature("length", FeatureType::SCALE, target_len);
        
        double distance = weighted_dist(obj, target);
        
        // Expected: sqrt((1.0 * norm(1,1)^2 + 0.5 * 2^2) / (1.0 + 0.5))
        // = sqrt((1.0 * 2 + 0.5 * 4) / 1.5) = sqrt(4 / 1.5) ≈ 1.633
        REQUIRE_THAT(distance, WithinAbs(1.633, 1e-2));
    }
    
    SECTION("Feature weighted distance with missing features") {
        std::unordered_map<std::string, double> weights;
        weights["position"] = 1.0;
        weights["length"] = 0.5;
        
        CostFunctions::FeatureWeightedDistance weighted_dist(weights);
        
        FeatureVector obj, target;
        
        // Both have position
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector2d target_pos(1.0, 1.0);
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        // Only obj has length (target missing)
        Eigen::VectorXd obj_len(1); obj_len[0] = 5.0;
        obj.addFeature("length", FeatureType::SCALE, obj_len);
        
        double distance = weighted_dist(obj, target);
        
        // Should only use position feature
        REQUIRE_THAT(distance, WithinAbs(std::sqrt(2.0), 1e-6));
    }
    
    SECTION("Feature weighted distance with zero weights") {
        std::unordered_map<std::string, double> weights;
        weights["position"] = 0.0;  // Zero weight - should be ignored
        weights["length"] = 1.0;
        
        CostFunctions::FeatureWeightedDistance weighted_dist(weights);
        
        FeatureVector obj, target;
        
        Eigen::Vector2d obj_pos(0.0, 0.0);
        Eigen::Vector2d target_pos(100.0, 100.0); // Large difference
        obj.addFeature("position", FeatureType::POSITION, obj_pos);
        target.addFeature("position", FeatureType::POSITION, target_pos);
        
        Eigen::VectorXd obj_len(1); obj_len[0] = 5.0;
        Eigen::VectorXd target_len(1); target_len[0] = 7.0; // Small difference
        obj.addFeature("length", FeatureType::SCALE, obj_len);
        target.addFeature("length", FeatureType::SCALE, target_len);
        
        double distance = weighted_dist(obj, target);
        
        // Should only use length (weight 1.0), ignore position (weight 0.0)
        REQUIRE_THAT(distance, WithinAbs(2.0, 1e-6));
    }
    
    SECTION("Feature weighted distance with no valid features") {
        std::unordered_map<std::string, double> weights;
        weights["nonexistent"] = 1.0;
        
        CostFunctions::FeatureWeightedDistance weighted_dist(weights);
        
        FeatureVector obj, target;
        Eigen::Vector2d pos(0.0, 0.0);
        obj.addFeature("position", FeatureType::POSITION, pos);
        target.addFeature("position", FeatureType::POSITION, pos);
        
        double distance = weighted_dist(obj, target);
        REQUIRE(distance == std::numeric_limits<double>::infinity());
    }
}

TEST_CASE("StateEstimation - Assignment - Assignment with custom cost function", "[Assignment]") {
    SECTION("Manhattan distance cost function") {
        auto manhattan_cost = [](FeatureVector const& obj, FeatureVector const& target) -> double {
            if (!obj.hasFeature("position") || !target.hasFeature("position")) {
                return std::numeric_limits<double>::infinity();
            }
            
            auto obj_pos = obj.getFeature("position");
            auto target_pos = target.getFeature("position");
            
            if (obj_pos.size() != target_pos.size()) {
                return std::numeric_limits<double>::infinity();
            }
            
            double manhattan_dist = 0.0;
            for (int i = 0; i < obj_pos.size(); ++i) {
                manhattan_dist += std::abs(obj_pos[i] - target_pos[i]);
            }
            return manhattan_dist;
        };
        
        HungarianAssignment assignment(manhattan_cost);
        
        std::vector<FeatureVector> objects;
        std::vector<FeatureVector> targets;
        
        // Object at (0, 0)
        FeatureVector obj0;
        Eigen::Vector2d obj_pos0(0.0, 0.0);
        obj0.addFeature("position", FeatureType::POSITION, obj_pos0);
        objects.push_back(obj0);
        
        // Target at (3, 4)
        FeatureVector target0;
        Eigen::Vector2d target_pos0(3.0, 4.0);
        target0.addFeature("position", FeatureType::POSITION, target_pos0);
        targets.push_back(target0);
        
        auto result = assignment.solve(objects, targets);
        
        REQUIRE(result.success == true);
        REQUIRE(result.assignments.size() == 1);
        REQUIRE(result.assignments[0] == 0);
        REQUIRE_THAT(result.costs[0], WithinAbs(7.0, 1e-6)); // |3-0| + |4-0| = 7
    }
}