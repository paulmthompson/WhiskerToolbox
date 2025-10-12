/**
 * @file mask_particle_filter_example.cpp
 * @brief Example demonstrating the use of MaskParticleFilter for tracking pixels through mask data
 * 
 * This example shows how to:
 * 1. Create synthetic mask data (moving circle)
 * 2. Track a single point through the masks
 * 3. Track multiple correlated points
 */

#include "StateEstimation/MaskParticleFilter.hpp"
#include "CoreGeometry/masks.hpp"

#include <iostream>
#include <vector>
#include <cmath>

using namespace StateEstimation;

/**
 * @brief Generate a circular mask centered at a given point
 */
Mask2D generateCircleMask(Point2D<uint32_t> center, float radius) {
    Mask2D mask;
    uint32_t r_sq = static_cast<uint32_t>(radius * radius);
    
    for (int32_t dx = -static_cast<int32_t>(radius); dx <= static_cast<int32_t>(radius); ++dx) {
        for (int32_t dy = -static_cast<int32_t>(radius); dy <= static_cast<int32_t>(radius); ++dy) {
            if (dx * dx + dy * dy <= static_cast<int32_t>(r_sq)) {
                uint32_t x = center.x + static_cast<uint32_t>(dx);
                uint32_t y = center.y + static_cast<uint32_t>(dy);
                mask.push_back({x, y});
            }
        }
    }
    
    return mask;
}

/**
 * @brief Example 1: Track a single point through moving masks
 */
void example_single_point_tracking() {
    std::cout << "\n=== Example 1: Single Point Tracking ===\n\n";
    
    // Create a moving circle trajectory (100 frames)
    size_t num_frames = 100;
    std::vector<Mask2D> masks;
    
    std::cout << "Generating synthetic data: circle moving from (100,100) to (300,100)\n";
    
    for (size_t i = 0; i < num_frames; ++i) {
        // Linear motion in x direction
        float t = static_cast<float>(i) / static_cast<float>(num_frames - 1);
        uint32_t x = 100 + static_cast<uint32_t>(200 * t);
        uint32_t y = 100;
        
        masks.push_back(generateCircleMask({x, y}, 25.0f));
    }
    
    // Ground truth labels (start and end)
    Point2D<uint32_t> start_label{100, 100};
    Point2D<uint32_t> end_label{300, 100};
    
    std::cout << "Start label: (" << start_label.x << ", " << start_label.y << ")\n";
    std::cout << "End label: (" << end_label.x << ", " << end_label.y << ")\n";
    
    // Create tracker
    MaskPointTracker tracker(
        1000,    // num_particles
        15.0f,   // transition_radius: allow up to 15 pixels movement per frame
        0.05f    // random_walk_prob: 5% chance of random exploration
    );
    
    std::cout << "\nTracking...\n";
    
    auto tracked_points = tracker.track(start_label, end_label, masks);
    
    std::cout << "Tracked " << tracked_points.size() << " frames\n";
    
    // Print some results
    std::cout << "\nSample tracked positions:\n";
    for (size_t i = 0; i < num_frames; i += 20) {
        std::cout << "  Frame " << i << ": (" 
                  << tracked_points[i].x << ", " 
                  << tracked_points[i].y << ")\n";
    }
    
    // Compute tracking error
    float total_error = 0.0f;
    for (size_t i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_frames - 1);
        uint32_t true_x = 100 + static_cast<uint32_t>(200 * t);
        uint32_t true_y = 100;
        
        float error = pointDistance(tracked_points[i], {true_x, true_y});
        total_error += error;
    }
    
    float avg_error = total_error / static_cast<float>(num_frames);
    std::cout << "\nAverage tracking error: " << avg_error << " pixels\n";
}

/**
 * @brief Example 2: Track multiple correlated points (whisker simulation)
 */
void example_correlated_tracking() {
    std::cout << "\n=== Example 2: Correlated Multi-Point Tracking ===\n\n";
    
    // Simulate a whisker with 3 tracked points: base, middle, tip
    size_t num_frames = 50;
    std::vector<Mask2D> masks;
    
    std::cout << "Generating synthetic data: 3 points moving together\n";
    
    for (size_t i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_frames - 1);
        
        // All three points move vertically
        uint32_t base_x = 100;
        uint32_t base_y = 100 + static_cast<uint32_t>(100 * t);
        
        uint32_t mid_x = 150;
        uint32_t mid_y = 120 + static_cast<uint32_t>(100 * t);
        
        uint32_t tip_x = 200;
        uint32_t tip_y = 140 + static_cast<uint32_t>(100 * t);
        
        // Create mask containing all three points
        Mask2D mask = generateCircleMask({base_x, base_y}, 20.0f);
        auto mask_mid = generateCircleMask({mid_x, mid_y}, 20.0f);
        auto mask_tip = generateCircleMask({tip_x, tip_y}, 20.0f);
        
        mask.insert(mask.end(), mask_mid.begin(), mask_mid.end());
        mask.insert(mask.end(), mask_tip.begin(), mask_tip.end());
        
        masks.push_back(mask);
    }
    
    // Ground truth labels for all three points
    std::vector<Point2D<uint32_t>> start_labels = {
        {100, 100},  // Base
        {150, 120},  // Middle
        {200, 140}   // Tip
    };
    
    std::vector<Point2D<uint32_t>> end_labels = {
        {100, 200},  // Base
        {150, 220},  // Middle
        {200, 240}   // Tip
    };
    
    std::cout << "Tracking 3 points:\n";
    std::cout << "  Point 0 (base): (" << start_labels[0].x << ", " << start_labels[0].y 
              << ") -> (" << end_labels[0].x << ", " << end_labels[0].y << ")\n";
    std::cout << "  Point 1 (mid):  (" << start_labels[1].x << ", " << start_labels[1].y 
              << ") -> (" << end_labels[1].x << ", " << end_labels[1].y << ")\n";
    std::cout << "  Point 2 (tip):  (" << start_labels[2].x << ", " << start_labels[2].y 
              << ") -> (" << end_labels[2].x << ", " << end_labels[2].y << ")\n";
    
    // Create correlated tracker
    CorrelatedMaskPointTracker tracker(
        1000,   // num_particles
        15.0f,  // transition_radius
        0.8f    // correlation_strength: high correlation (points move together)
    );
    
    std::cout << "\nTracking with correlation constraint...\n";
    
    auto tracked_states = tracker.track(start_labels, end_labels, masks);
    
    std::cout << "Tracked " << tracked_states.size() << " frames\n";
    
    // Check spacing consistency
    float initial_spacing_01 = pointDistance(start_labels[0], start_labels[1]);
    float initial_spacing_12 = pointDistance(start_labels[1], start_labels[2]);
    
    std::cout << "\nInitial spacing:\n";
    std::cout << "  Points 0-1: " << initial_spacing_01 << " pixels\n";
    std::cout << "  Points 1-2: " << initial_spacing_12 << " pixels\n";
    
    float avg_spacing_01 = 0.0f;
    float avg_spacing_12 = 0.0f;
    
    for (auto const& state : tracked_states) {
        avg_spacing_01 += pointDistance(state.points[0], state.points[1]);
        avg_spacing_12 += pointDistance(state.points[1], state.points[2]);
    }
    
    avg_spacing_01 /= static_cast<float>(num_frames);
    avg_spacing_12 /= static_cast<float>(num_frames);
    
    std::cout << "\nAverage tracked spacing:\n";
    std::cout << "  Points 0-1: " << avg_spacing_01 << " pixels\n";
    std::cout << "  Points 1-2: " << avg_spacing_12 << " pixels\n";
    
    std::cout << "\nSpacing consistency (should be close to initial):\n";
    std::cout << "  Deviation 0-1: " << std::abs(avg_spacing_01 - initial_spacing_01) << " pixels\n";
    std::cout << "  Deviation 1-2: " << std::abs(avg_spacing_12 - initial_spacing_12) << " pixels\n";
}

/**
 * @brief Example 3: Handling ambiguous cases
 */
void example_ambiguous_tracking() {
    std::cout << "\n=== Example 3: Ambiguous Case (Large Mask) ===\n\n";
    
    // Create a scenario with a large mask containing many possible paths
    size_t num_frames = 30;
    std::vector<Mask2D> masks;
    
    std::cout << "Generating ambiguous data: large overlapping circles\n";
    
    for (size_t i = 0; i < num_frames; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_frames - 1);
        
        // Two possible paths
        uint32_t path1_x = 100 + static_cast<uint32_t>(100 * t);
        uint32_t path1_y = 100;
        
        uint32_t path2_x = 100 + static_cast<uint32_t>(100 * t);
        uint32_t path2_y = 150;
        
        // Create large mask containing both paths
        Mask2D mask = generateCircleMask({path1_x, path1_y}, 40.0f);
        auto mask2 = generateCircleMask({path2_x, path2_y}, 40.0f);
        mask.insert(mask.end(), mask2.begin(), mask2.end());
        
        masks.push_back(mask);
    }
    
    // Ground truth on upper path
    Point2D<uint32_t> start_label{100, 100};
    Point2D<uint32_t> end_label{200, 100};
    
    std::cout << "Start label: (" << start_label.x << ", " << start_label.y << ")\n";
    std::cout << "End label: (" << end_label.x << ", " << end_label.y << ")\n";
    std::cout << "Alternative path at y=150 also present in masks\n";
    
    // Create tracker with more particles for ambiguous case
    MaskPointTracker tracker(
        2000,    // More particles for ambiguous cases
        20.0f,   // Larger radius to handle uncertainty
        0.1f     // Higher random walk for exploration
    );
    
    std::cout << "\nTracking with increased particles...\n";
    
    auto tracked_points = tracker.track(start_label, end_label, masks);
    
    // Check if it stayed on the correct path
    int correct_path_count = 0;
    for (auto const& pt : tracked_points) {
        if (std::abs(static_cast<int32_t>(pt.y) - 100) < 30) {
            correct_path_count++;
        }
    }
    
    float accuracy = 100.0f * static_cast<float>(correct_path_count) / static_cast<float>(num_frames);
    
    std::cout << "Stayed on correct path: " << correct_path_count << "/" << num_frames 
              << " frames (" << accuracy << "%)\n";
}

int main() {
    std::cout << "======================================\n";
    std::cout << "  Mask Particle Filter Examples\n";
    std::cout << "======================================\n";
    
    try {
        example_single_point_tracking();
        example_correlated_tracking();
        example_ambiguous_tracking();
        
        std::cout << "\n======================================\n";
        std::cout << "  All examples completed successfully!\n";
        std::cout << "======================================\n";
        
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
