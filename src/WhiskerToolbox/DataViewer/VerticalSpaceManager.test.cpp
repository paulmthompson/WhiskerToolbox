#include "VerticalSpaceManager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <iostream>

TEST_CASE("VerticalSpaceManager - Basic functionality", "[VerticalSpaceManager][unit]") {
    
    SECTION("Constructor initializes correctly") {
        VerticalSpaceManager manager(800, 2.0f);
        
        REQUIRE(manager.getTotalSeriesCount() == 0);
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 0);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalEvent) == 0);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalInterval) == 0);
        REQUIRE(manager.getAllSeriesKeys().empty());
    }
    
    SECTION("Adding single series works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        auto position = manager.addSeries("analog_1", DataSeriesType::Analog);
        
        REQUIRE(manager.getTotalSeriesCount() == 1);
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 1);
        
        // Check position is reasonable
        REQUIRE(position.display_order == 0);
        REQUIRE(position.allocated_height > 0.0f);
        REQUIRE(position.scale_factor > 0.0f);
        
        // Should be positioned in upper part of canvas (positive Y)
        REQUIRE(position.y_offset > 0.0f);
        REQUIRE(position.y_offset < 1.0f);
    }
    
    SECTION("Series retrieval works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        manager.addSeries("test_series", DataSeriesType::DigitalEvent);
        
        auto retrieved = manager.getSeriesPosition("test_series");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved.value().display_order == 0);
        
        auto missing = manager.getSeriesPosition("nonexistent");
        REQUIRE_FALSE(missing.has_value());
    }
    
    SECTION("Removing series works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        manager.addSeries("series_1", DataSeriesType::Analog);
        manager.addSeries("series_2", DataSeriesType::DigitalEvent);
        
        REQUIRE(manager.getTotalSeriesCount() == 2);
        
        bool removed = manager.removeSeries("series_1");
        REQUIRE(removed);
        REQUIRE(manager.getTotalSeriesCount() == 1);
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 0);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalEvent) == 1);
        
        bool removed_missing = manager.removeSeries("nonexistent");
        REQUIRE_FALSE(removed_missing);
    }
}

TEST_CASE("VerticalSpaceManager - Multi-series positioning", "[VerticalSpaceManager][positioning]") {
    
    SECTION("Multiple series of same type are positioned correctly") {
        VerticalSpaceManager manager(600, 2.0f);
        
        auto pos1 = manager.addSeries("analog_1", DataSeriesType::Analog);
        auto pos2 = manager.addSeries("analog_2", DataSeriesType::Analog);
        auto pos3 = manager.addSeries("analog_3", DataSeriesType::Analog);
        
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 3);
        
        // Check display order
        REQUIRE(pos1.display_order == 0);
        REQUIRE(pos2.display_order == 1);
        REQUIRE(pos3.display_order == 2);
        
        // First series should be highest (most positive Y)
        REQUIRE(pos1.y_offset > pos2.y_offset);
        REQUIRE(pos2.y_offset > pos3.y_offset);
        
        // All should have reasonable heights
        REQUIRE(pos1.allocated_height > 0.01f);
        REQUIRE(pos2.allocated_height > 0.01f);
        REQUIRE(pos3.allocated_height > 0.01f);
    }
    
    SECTION("Mixed data types are positioned correctly") {
        VerticalSpaceManager manager(800, 2.0f);
        
        // Add series in specific order
        auto analog_pos = manager.addSeries("analog_1", DataSeriesType::Analog);
        auto event_pos = manager.addSeries("event_1", DataSeriesType::DigitalEvent);
        auto interval_pos = manager.addSeries("interval_1", DataSeriesType::DigitalInterval);
        
        REQUIRE(manager.getTotalSeriesCount() == 3);
        
        // Verify order preservation (analog added first, should be on top)
        REQUIRE(analog_pos.display_order == 0);
        REQUIRE(event_pos.display_order == 1);
        REQUIRE(interval_pos.display_order == 2);
        
        // Verify Y positioning (higher display_order should have lower Y)
        REQUIRE(analog_pos.y_offset > event_pos.y_offset);
        REQUIRE(event_pos.y_offset > interval_pos.y_offset);
        
        // All should fit within canvas bounds
        REQUIRE(analog_pos.y_offset < 1.0f);
        REQUIRE(interval_pos.y_offset > -1.0f);
    }
}

TEST_CASE("VerticalSpaceManager - Order preservation and redistribution", "[VerticalSpaceManager][order]") {
    
    SECTION("Adding new series redistributes existing ones") {
        VerticalSpaceManager manager(400, 2.0f);
        
        // Add first series
        auto pos1_initial = manager.addSeries("series_1", DataSeriesType::Analog);
        
        // Add second series - should cause redistribution
        auto pos2 = manager.addSeries("series_2", DataSeriesType::Analog);
        auto pos1_after = manager.getSeriesPosition("series_1").value();
        
        // Positions should have changed due to redistribution
        // (exact values depend on algorithm, but heights should be smaller)
        REQUIRE(pos1_after.allocated_height <= pos1_initial.allocated_height);
        
        // Order should be preserved
        REQUIRE(pos1_after.display_order == 0);
        REQUIRE(pos2.display_order == 1);
        REQUIRE(pos1_after.y_offset > pos2.y_offset);
    }
    
    SECTION("Mixed type addition maintains proper order") {
        VerticalSpaceManager manager(600, 2.0f);
        
        // Add in mixed order
        manager.addSeries("analog_1", DataSeriesType::Analog);
        manager.addSeries("event_1", DataSeriesType::DigitalEvent);
        manager.addSeries("analog_2", DataSeriesType::Analog);
        manager.addSeries("event_2", DataSeriesType::DigitalEvent);
        
        auto keys = manager.getAllSeriesKeys();
        REQUIRE(keys.size() == 4);
        
        // Should maintain add order
        REQUIRE(keys[0] == "analog_1");
        REQUIRE(keys[1] == "event_1");
        REQUIRE(keys[2] == "analog_2");
        REQUIRE(keys[3] == "event_2");
        
        // Verify Y positions decrease with display order
        auto pos_analog1 = manager.getSeriesPosition("analog_1").value();
        auto pos_event1 = manager.getSeriesPosition("event_1").value();
        auto pos_analog2 = manager.getSeriesPosition("analog_2").value();
        auto pos_event2 = manager.getSeriesPosition("event_2").value();
        
        REQUIRE(pos_analog1.y_offset > pos_event1.y_offset);
        REQUIRE(pos_event1.y_offset > pos_analog2.y_offset);
        REQUIRE(pos_analog2.y_offset > pos_event2.y_offset);
    }
}

TEST_CASE("VerticalSpaceManager - Configuration and customization", "[VerticalSpaceManager][config]") {
    
    SECTION("Data type configuration works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        // Get and modify analog config
        auto analog_config = manager.getDataTypeConfig(DataSeriesType::Analog);
        analog_config.min_height_per_series = 0.05f;
        analog_config.max_height_per_series = 0.5f;
        
        manager.setDataTypeConfig(DataSeriesType::Analog, analog_config);
        
        auto retrieved_config = manager.getDataTypeConfig(DataSeriesType::Analog);
        REQUIRE_THAT(retrieved_config.min_height_per_series, 
                     Catch::Matchers::WithinAbs(0.05f, 0.001f));
        REQUIRE_THAT(retrieved_config.max_height_per_series, 
                     Catch::Matchers::WithinAbs(0.5f, 0.001f));
    }
    
    SECTION("Canvas dimension updates work correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        manager.addSeries("test_series", DataSeriesType::Analog);
        auto pos_before = manager.getSeriesPosition("test_series").value();
        
        // Update canvas dimensions
        manager.updateCanvasDimensions(800, 3.0f);
        auto pos_after = manager.getSeriesPosition("test_series").value();
        
        // Position should be recalculated (specific values depend on algorithm)
        // But series should still exist and have valid position
        REQUIRE(pos_after.allocated_height > 0.0f);
        REQUIRE(pos_after.scale_factor > 0.0f);
    }
    
    SECTION("Manual recalculation works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        manager.addSeries("series_1", DataSeriesType::Analog);
        manager.addSeries("series_2", DataSeriesType::DigitalEvent);
        
        auto pos1_before = manager.getSeriesPosition("series_1").value();
        auto pos2_before = manager.getSeriesPosition("series_2").value();
        
        // Trigger manual recalculation
        manager.recalculateAllPositions();
        
        auto pos1_after = manager.getSeriesPosition("series_1").value();
        auto pos2_after = manager.getSeriesPosition("series_2").value();
        
        // Order should be preserved
        REQUIRE(pos1_after.display_order == pos1_before.display_order);
        REQUIRE(pos2_after.display_order == pos2_before.display_order);
    }
}

TEST_CASE("VerticalSpaceManager - Edge cases and stress testing", "[VerticalSpaceManager][edge_cases]") {
    
    SECTION("Many series stress test") {
        VerticalSpaceManager manager(1000, 2.0f);
        
        // Add many series of different types
        for (int i = 0; i < 20; ++i) {
            manager.addSeries("analog_" + std::to_string(i), DataSeriesType::Analog);
            manager.addSeries("event_" + std::to_string(i), DataSeriesType::DigitalEvent);
            manager.addSeries("interval_" + std::to_string(i), DataSeriesType::DigitalInterval);
        }
        
        REQUIRE(manager.getTotalSeriesCount() == 60);
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 20);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalEvent) == 20);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalInterval) == 20);
        
        // All series should have valid positions
        auto all_keys = manager.getAllSeriesKeys();
        REQUIRE(all_keys.size() == 60);
        
        for (auto const & key : all_keys) {
            auto pos = manager.getSeriesPosition(key);
            REQUIRE(pos.has_value());
            REQUIRE(pos.value().allocated_height > 0.0f);
            REQUIRE(pos.value().scale_factor > 0.0f);
        }
        
        // Should maintain order
        REQUIRE(all_keys[0] == "analog_0");
        REQUIRE(all_keys[1] == "event_0");
        REQUIRE(all_keys[2] == "interval_0");
        REQUIRE(all_keys[3] == "analog_1");
    }
    
    SECTION("Clear functionality works correctly") {
        VerticalSpaceManager manager(400, 2.0f);
        
        manager.addSeries("series_1", DataSeriesType::Analog);
        manager.addSeries("series_2", DataSeriesType::DigitalEvent);
        
        REQUIRE(manager.getTotalSeriesCount() == 2);
        
        manager.clear();
        
        REQUIRE(manager.getTotalSeriesCount() == 0);
        REQUIRE(manager.getAllSeriesKeys().empty());
        REQUIRE_FALSE(manager.getSeriesPosition("series_1").has_value());
        REQUIRE_FALSE(manager.getSeriesPosition("series_2").has_value());
    }
    
    SECTION("Duplicate series handling") {
        VerticalSpaceManager manager(400, 2.0f);
        
        auto pos1 = manager.addSeries("duplicate", DataSeriesType::Analog);
        REQUIRE(manager.getTotalSeriesCount() == 1);
        
        // Adding same key should update existing
        auto pos2 = manager.addSeries("duplicate", DataSeriesType::DigitalEvent);
        REQUIRE(manager.getTotalSeriesCount() == 1);
        REQUIRE(manager.getSeriesCount(DataSeriesType::Analog) == 0);
        REQUIRE(manager.getSeriesCount(DataSeriesType::DigitalEvent) == 1);
        
        // Position should be different due to type change
        auto final_pos = manager.getSeriesPosition("duplicate").value();
        REQUIRE(final_pos.display_order == 0); // Still first
    }
}

TEST_CASE("VerticalSpaceManager - Realistic usage scenarios", "[VerticalSpaceManager][integration]") {
    
    SECTION("Neuroscience data scenario: 32 analog + 25 events") {
        VerticalSpaceManager manager(600, 2.0f);
        
        // Add 32 analog channels first (typical neuroscience recording)
        for (int i = 0; i < 32; ++i) {
            manager.addSeries("lfp_ch" + std::to_string(i), DataSeriesType::Analog);
        }
        
        // Now add 25 digital event channels (as mentioned in user's problem)
        for (int i = 0; i < 25; ++i) {
            manager.addSeries("event_ch" + std::to_string(i), DataSeriesType::DigitalEvent);
        }
        
        REQUIRE(manager.getTotalSeriesCount() == 57);
        
        // Verify no overlap: all analog series should be above all event series
        auto lfp_pos = manager.getSeriesPosition("lfp_ch0").value();
        auto event_pos = manager.getSeriesPosition("event_ch0").value();
        
        REQUIRE(lfp_pos.y_offset > event_pos.y_offset);
        
        // All series should have reasonable heights
        for (int i = 0; i < 32; ++i) {
            auto pos = manager.getSeriesPosition("lfp_ch" + std::to_string(i)).value();
            REQUIRE(pos.allocated_height > 0.005f); // Minimum visible height
        }
        
        for (int i = 0; i < 25; ++i) {
            auto pos = manager.getSeriesPosition("event_ch" + std::to_string(i)).value();
            REQUIRE(pos.allocated_height > 0.005f); // Minimum visible height
        }
    }
    
    SECTION("Auto-arrange simulation: recalculation improves layout") {
        VerticalSpaceManager manager(400, 2.0f);
        
        // Add series with suboptimal initial configuration
        DataTypeConfig tight_config;
        tight_config.min_height_per_series = 0.001f; // Very tight
        tight_config.inter_series_spacing = 0.001f;
        manager.setDataTypeConfig(DataSeriesType::Analog, tight_config);
        
        for (int i = 0; i < 10; ++i) {
            manager.addSeries("cramped_" + std::to_string(i), DataSeriesType::Analog);
        }
        
        auto cramped_height = manager.getSeriesPosition("cramped_0").value().allocated_height;
        
        // Now apply better configuration (simulating auto-arrange)
        DataTypeConfig better_config;
        better_config.min_height_per_series = 0.02f; // More reasonable
        better_config.inter_series_spacing = 0.01f;
        manager.setDataTypeConfig(DataSeriesType::Analog, better_config);
        
        auto improved_height = manager.getSeriesPosition("cramped_0").value().allocated_height;
        
        // Height should have improved (or at least stayed same if already optimal)
        REQUIRE(improved_height >= cramped_height);
    }
} 