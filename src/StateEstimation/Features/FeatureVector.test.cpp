#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Features/FeatureVector.hpp"

#include <Eigen/Dense>

using namespace StateEstimation;
using Catch::Matchers::WithinAbs;

TEST_CASE("StateEstimation - FeatureVector - FeatureDescriptor", "[FeatureVector]") {
    SECTION("Construction") {
        FeatureDescriptor desc("position", FeatureType::POSITION, 0, 2, true);
        
        REQUIRE(desc.name == "position");
        REQUIRE(desc.type == FeatureType::POSITION);
        REQUIRE(desc.start_index == 0);
        REQUIRE(desc.size == 2);
        REQUIRE(desc.has_derivatives == true);
    }
}

TEST_CASE("StateEstimation - FeatureVector", "[FeatureVector]") {
    SECTION("Default construction") {
        FeatureVector fv;
        
        REQUIRE(fv.getFeatureCount() == 0);
        REQUIRE(fv.getDimension() == 0);
        REQUIRE(fv.getVector().size() == 0);
    }
    
    SECTION("Construction with capacity") {
        FeatureVector fv(10);
        
        REQUIRE(fv.getFeatureCount() == 0);
        REQUIRE(fv.getDimension() == 10); // Initial capacity
        REQUIRE(fv.getVector().size() == 10);
    }
    
    SECTION("Add single feature") {
        FeatureVector fv;
        
        Eigen::Vector2d position(1.0, 2.0);
        std::size_t index = fv.addFeature("position", FeatureType::POSITION, position, true);
        
        REQUIRE(index == 0);
        REQUIRE(fv.getFeatureCount() == 1);
        REQUIRE(fv.getDimension() == 2);
        REQUIRE(fv.hasFeature("position"));
        REQUIRE_FALSE(fv.hasFeature("velocity"));
        
        auto retrieved = fv.getFeature("position");
        REQUIRE_THAT(retrieved[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(retrieved[1], WithinAbs(2.0, 1e-6));
    }
    
    SECTION("Add multiple features") {
        FeatureVector fv;
        
        // Add position feature
        Eigen::Vector2d position(5.0, 10.0);
        fv.addFeature("position", FeatureType::POSITION, position, true);
        
        // Add length feature
        Eigen::VectorXd length(1);
        length[0] = 15.0;
        fv.addFeature("length", FeatureType::SCALE, length, false);
        
        // Add orientation feature
        Eigen::VectorXd orientation(1);
        orientation[0] = 0.5;
        fv.addFeature("orientation", FeatureType::ORIENTATION, orientation, false);
        
        REQUIRE(fv.getFeatureCount() == 3);
        REQUIRE(fv.getDimension() == 4); // 2 + 1 + 1
        
        REQUIRE(fv.hasFeature("position"));
        REQUIRE(fv.hasFeature("length"));
        REQUIRE(fv.hasFeature("orientation"));
        
        // Check feature values
        auto pos = fv.getFeature("position");
        REQUIRE_THAT(pos[0], WithinAbs(5.0, 1e-6));
        REQUIRE_THAT(pos[1], WithinAbs(10.0, 1e-6));
        
        auto len = fv.getFeature("length");
        REQUIRE_THAT(len[0], WithinAbs(15.0, 1e-6));
        
        auto ori = fv.getFeature("orientation");
        REQUIRE_THAT(ori[0], WithinAbs(0.5, 1e-6));
    }
    
    SECTION("Get feature by index") {
        FeatureVector fv;
        
        Eigen::Vector2d pos1(1.0, 2.0);
        Eigen::Vector2d pos2(3.0, 4.0);
        
        fv.addFeature("pos1", FeatureType::POSITION, pos1);
        fv.addFeature("pos2", FeatureType::POSITION, pos2);
        
        auto feature0 = fv.getFeature(0);
        auto feature1 = fv.getFeature(1);
        
        REQUIRE_THAT(feature0[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(feature0[1], WithinAbs(2.0, 1e-6));
        REQUIRE_THAT(feature1[0], WithinAbs(3.0, 1e-6));
        REQUIRE_THAT(feature1[1], WithinAbs(4.0, 1e-6));
    }
    
    SECTION("Set feature values") {
        FeatureVector fv;
        
        // Add initial feature
        Eigen::Vector2d initial_pos(0.0, 0.0);
        fv.addFeature("position", FeatureType::POSITION, initial_pos);
        
        // Update feature value
        Eigen::Vector2d new_pos(10.0, 20.0);
        fv.setFeature("position", new_pos);
        
        auto retrieved = fv.getFeature("position");
        REQUIRE_THAT(retrieved[0], WithinAbs(10.0, 1e-6));
        REQUIRE_THAT(retrieved[1], WithinAbs(20.0, 1e-6));
        
        // Test set by index
        Eigen::Vector2d newer_pos(30.0, 40.0);
        fv.setFeature(0, newer_pos);
        
        retrieved = fv.getFeature("position");
        REQUIRE_THAT(retrieved[0], WithinAbs(30.0, 1e-6));
        REQUIRE_THAT(retrieved[1], WithinAbs(40.0, 1e-6));
    }
    
    SECTION("Feature descriptors") {
        FeatureVector fv;
        
        Eigen::Vector2d position(1.0, 2.0);
        fv.addFeature("position", FeatureType::POSITION, position, true);
        
        // Get descriptor by name
        auto* desc_ptr = fv.getFeatureDescriptor("position");
        REQUIRE(desc_ptr != nullptr);
        REQUIRE(desc_ptr->name == "position");
        REQUIRE(desc_ptr->type == FeatureType::POSITION);
        REQUIRE(desc_ptr->start_index == 0);
        REQUIRE(desc_ptr->size == 2);
        REQUIRE(desc_ptr->has_derivatives == true);
        
        // Get descriptor by index
        auto const& desc_ref = fv.getFeatureDescriptor(0);
        REQUIRE(desc_ref.name == "position");
        REQUIRE(desc_ref.type == FeatureType::POSITION);
        
        // Non-existent feature should return nullptr
        auto* null_desc = fv.getFeatureDescriptor("nonexistent");
        REQUIRE(null_desc == nullptr);
    }
    
    SECTION("Get all descriptors") {
        FeatureVector fv;
        
        Eigen::Vector2d pos(1.0, 2.0);
        Eigen::VectorXd len(1);
        len[0] = 5.0;
        
        fv.addFeature("position", FeatureType::POSITION, pos);
        fv.addFeature("length", FeatureType::SCALE, len);
        
        auto descriptors = fv.getFeatureDescriptors();
        REQUIRE(descriptors.size() == 2);
        
        REQUIRE(descriptors[0].name == "position");
        REQUIRE(descriptors[1].name == "length");
    }
    
    SECTION("Clear features") {
        FeatureVector fv;
        
        Eigen::Vector2d pos(1.0, 2.0);
        fv.addFeature("position", FeatureType::POSITION, pos);
        
        REQUIRE(fv.getFeatureCount() == 1);
        REQUIRE(fv.getDimension() == 2);
        
        fv.clear();
        
        REQUIRE(fv.getFeatureCount() == 0);
        REQUIRE(fv.getDimension() == 0);
        REQUIRE_FALSE(fv.hasFeature("position"));
    }
    
    SECTION("Feature subset") {
        FeatureVector fv;
        
        Eigen::Vector2d pos(1.0, 2.0);
        Eigen::VectorXd len(1);
        len[0] = 10.0;
        Eigen::VectorXd ori(1);
        ori[0] = 0.5;
        
        fv.addFeature("position", FeatureType::POSITION, pos);
        fv.addFeature("length", FeatureType::SCALE, len);
        fv.addFeature("orientation", FeatureType::ORIENTATION, ori);
        
        // Create subset with only position and orientation
        std::vector<std::string> subset_names = {"position", "orientation"};
        auto subset = fv.subset(subset_names);
        
        REQUIRE(subset.getFeatureCount() == 2);
        REQUIRE(subset.hasFeature("position"));
        REQUIRE(subset.hasFeature("orientation"));
        REQUIRE_FALSE(subset.hasFeature("length"));
        
        auto subset_pos = subset.getFeature("position");
        auto subset_ori = subset.getFeature("orientation");
        
        REQUIRE_THAT(subset_pos[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(subset_pos[1], WithinAbs(2.0, 1e-6));
        REQUIRE_THAT(subset_ori[0], WithinAbs(0.5, 1e-6));
    }
    
    SECTION("Error cases") {
        FeatureVector fv;
        
        // Add a feature
        Eigen::Vector2d pos(1.0, 2.0);
        fv.addFeature("position", FeatureType::POSITION, pos);
        
        // Try to add duplicate feature
        REQUIRE_THROWS_AS(fv.addFeature("position", FeatureType::POSITION, pos), std::invalid_argument);
        
        // Try to get non-existent feature
        REQUIRE_THROWS_AS(fv.getFeature("nonexistent"), std::invalid_argument);
        REQUIRE_THROWS_AS(fv.getFeature(10), std::out_of_range);
        
        // Try to set non-existent feature
        REQUIRE_THROWS_AS(fv.setFeature("nonexistent", pos), std::invalid_argument);
        REQUIRE_THROWS_AS(fv.setFeature(10, pos), std::out_of_range);
        
        // Try to set feature with wrong size
        Eigen::VectorXd wrong_size(3);
        wrong_size << 1.0, 2.0, 3.0;
        REQUIRE_THROWS_AS(fv.setFeature("position", wrong_size), std::invalid_argument);
        
        // Try to get descriptor for invalid index
        REQUIRE_THROWS_AS(fv.getFeatureDescriptor(10), std::out_of_range);
    }
    
    SECTION("Complete vector access") {
        FeatureVector fv;
        
        Eigen::Vector2d pos(1.0, 2.0);
        Eigen::VectorXd len(1);
        len[0] = 5.0;
        
        fv.addFeature("position", FeatureType::POSITION, pos);
        fv.addFeature("length", FeatureType::SCALE, len);
        
        // Get complete vector
        auto complete_vector = fv.getVector();
        REQUIRE(complete_vector.size() == 3);
        REQUIRE_THAT(complete_vector[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(complete_vector[1], WithinAbs(2.0, 1e-6));
        REQUIRE_THAT(complete_vector[2], WithinAbs(5.0, 1e-6));
        
        // Modify complete vector
        auto& mutable_vector = fv.getVector();
        mutable_vector[0] = 10.0;
        mutable_vector[1] = 20.0;
        mutable_vector[2] = 50.0;
        
        // Check that individual features are updated
        auto updated_pos = fv.getFeature("position");
        auto updated_len = fv.getFeature("length");
        
        REQUIRE_THAT(updated_pos[0], WithinAbs(10.0, 1e-6));
        REQUIRE_THAT(updated_pos[1], WithinAbs(20.0, 1e-6));
        REQUIRE_THAT(updated_len[0], WithinAbs(50.0, 1e-6));
    }
}

TEST_CASE("State Estimation - FeatureVector - FeatureVector complex scenarios", "[FeatureVector]") {
    SECTION("Large feature vector") {
        FeatureVector fv;
        
        // Add many features of different types
        for (int i = 0; i < 10; ++i) {
            Eigen::Vector2d pos(static_cast<double>(i), static_cast<double>(i + 1));
            fv.addFeature("pos_" + std::to_string(i), FeatureType::POSITION, pos);
            
            Eigen::VectorXd scalar(1);
            scalar[0] = static_cast<double>(i * 10);
            fv.addFeature("scalar_" + std::to_string(i), FeatureType::SCALE, scalar);
        }
        
        REQUIRE(fv.getFeatureCount() == 20);
        REQUIRE(fv.getDimension() == 30); // 10 * 2 + 10 * 1
        
        // Check first and last features
        auto first_pos = fv.getFeature("pos_0");
        auto last_scalar = fv.getFeature("scalar_9");
        
        REQUIRE_THAT(first_pos[0], WithinAbs(0.0, 1e-6));
        REQUIRE_THAT(first_pos[1], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(last_scalar[0], WithinAbs(90.0, 1e-6));
    }
    
    SECTION("Mixed feature types") {
        FeatureVector fv;
        
        // Position feature (2D, has derivatives)
        Eigen::Vector2d position(10.0, 20.0);
        fv.addFeature("position", FeatureType::POSITION, position, true);
        
        // Orientation feature (1D, no derivatives)
        Eigen::VectorXd orientation(1);
        orientation[0] = 1.57; // π/2
        fv.addFeature("orientation", FeatureType::ORIENTATION, orientation, false);
        
        // Scale feature (1D, no derivatives)
        Eigen::VectorXd scale(1);
        scale[0] = 2.5;
        fv.addFeature("scale", FeatureType::SCALE, scale, false);
        
        // Intensity feature (1D, no derivatives)
        Eigen::VectorXd intensity(1);
        intensity[0] = 128.0;
        fv.addFeature("intensity", FeatureType::INTENSITY, intensity, false);
        
        // Shape feature (3D, no derivatives)
        Eigen::Vector3d shape(1.0, 0.8, 1.2);
        fv.addFeature("shape", FeatureType::SHAPE, shape, false);
        
        REQUIRE(fv.getFeatureCount() == 5);
        REQUIRE(fv.getDimension() == 8); // 2 + 1 + 1 + 1 + 3
        
        // Check feature types through descriptors
        auto pos_desc = fv.getFeatureDescriptor("position");
        auto ori_desc = fv.getFeatureDescriptor("orientation");
        auto shape_desc = fv.getFeatureDescriptor("shape");
        
        REQUIRE(pos_desc->type == FeatureType::POSITION);
        REQUIRE(pos_desc->has_derivatives == true);
        REQUIRE(ori_desc->type == FeatureType::ORIENTATION);
        REQUIRE(ori_desc->has_derivatives == false);
        REQUIRE(shape_desc->type == FeatureType::SHAPE);
        REQUIRE(shape_desc->size == 3);
        
        // Verify values
        auto retrieved_shape = fv.getFeature("shape");
        REQUIRE_THAT(retrieved_shape[0], WithinAbs(1.0, 1e-6));
        REQUIRE_THAT(retrieved_shape[1], WithinAbs(0.8, 1e-6));
        REQUIRE_THAT(retrieved_shape[2], WithinAbs(1.2, 1e-6));
    }
    
    SECTION("Feature ordering and indexing") {
        FeatureVector fv;
        
        // Add features in specific order
        Eigen::VectorXd scalar1(1); scalar1[0] = 1.0;
        Eigen::Vector2d vector2(2.0, 3.0);
        Eigen::VectorXd scalar2(1); scalar2[0] = 4.0;
        
        fv.addFeature("first", FeatureType::SCALE, scalar1);
        fv.addFeature("second", FeatureType::POSITION, vector2);
        fv.addFeature("third", FeatureType::SCALE, scalar2);
        
        // Check that complete vector has correct ordering
        auto complete = fv.getVector();
        REQUIRE(complete.size() == 4);
        REQUIRE_THAT(complete[0], WithinAbs(1.0, 1e-6)); // first[0]
        REQUIRE_THAT(complete[1], WithinAbs(2.0, 1e-6)); // second[0]
        REQUIRE_THAT(complete[2], WithinAbs(3.0, 1e-6)); // second[1]
        REQUIRE_THAT(complete[3], WithinAbs(4.0, 1e-6)); // third[0]
        
        // Check descriptors have correct start indices
        auto first_desc = fv.getFeatureDescriptor("first");
        auto second_desc = fv.getFeatureDescriptor("second");
        auto third_desc = fv.getFeatureDescriptor("third");
        
        REQUIRE(first_desc->start_index == 0);
        REQUIRE(second_desc->start_index == 1);
        REQUIRE(third_desc->start_index == 3);
    }
}