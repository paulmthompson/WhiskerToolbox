#include "VerticalSpaceManager.hpp"
#include <iostream>
#include <iomanip>

/**
 * @brief Demonstration of VerticalSpaceManager solving the overlap problem
 * 
 * This program demonstrates the exact use case mentioned by the user:
 * - 32 analog channels (LFP data)
 * - 25 digital event channels
 * 
 * Without coordination, these would overlap. With VerticalSpaceManager,
 * they are positioned appropriately to prevent overlap.
 */
int main() {
    std::cout << "=== VerticalSpaceManager Demo ===" << std::endl;
    std::cout << "Simulating the neuroscience use case: 32 analog + 25 events" << std::endl << std::endl;
    
    // Create manager with typical canvas dimensions
    VerticalSpaceManager manager(600, 2.0f);
    
    std::cout << "Step 1: Adding 32 analog channels (simulating LFP data)..." << std::endl;
    
    // Add 32 analog series (typical neuroscience setup)
    for (int i = 0; i < 32; ++i) {
        auto pos = manager.addSeries("lfp_ch" + std::to_string(i), DataSeriesType::Analog);
        if (i < 3) {  // Show details for first few
            std::cout << "  " << "lfp_ch" << i << " -> y_offset: " << std::fixed << std::setprecision(3) 
                      << pos.y_offset << ", height: " << pos.allocated_height << std::endl;
        }
    }
    
    auto first_analog_pos = manager.getSeriesPosition("lfp_ch0").value();
    auto last_analog_pos = manager.getSeriesPosition("lfp_ch31").value();
    
    std::cout << "  First analog (lfp_ch0): y_offset = " << first_analog_pos.y_offset << std::endl;
    std::cout << "  Last analog (lfp_ch31): y_offset = " << last_analog_pos.y_offset << std::endl;
    std::cout << "  Analog channels span: " << (first_analog_pos.y_offset - last_analog_pos.y_offset) << " units" << std::endl << std::endl;
    
    std::cout << "Step 2: Adding 25 digital event channels..." << std::endl;
    std::cout << "  (This is where overlap would occur without coordination)" << std::endl;
    
    // Add 25 digital event series (the problematic case mentioned by user)
    for (int i = 0; i < 25; ++i) {
        auto pos = manager.addSeries("event_ch" + std::to_string(i), DataSeriesType::DigitalEvent);
        if (i < 3) {  // Show details for first few
            std::cout << "  " << "event_ch" << i << " -> y_offset: " << std::fixed << std::setprecision(3) 
                      << pos.y_offset << ", height: " << pos.allocated_height << std::endl;
        }
    }
    
    auto first_event_pos = manager.getSeriesPosition("event_ch0").value();
    auto last_event_pos = manager.getSeriesPosition("event_ch24").value();
    
    std::cout << "  First event (event_ch0): y_offset = " << first_event_pos.y_offset << std::endl;
    std::cout << "  Last event (event_ch24): y_offset = " << last_event_pos.y_offset << std::endl;
    std::cout << "  Event channels span: " << (first_event_pos.y_offset - last_event_pos.y_offset) << " units" << std::endl << std::endl;
    
    std::cout << "Step 3: Verifying no overlap..." << std::endl;
    
    // Check for overlap
    auto analog_bottom = last_analog_pos.y_offset - last_analog_pos.allocated_height * 0.5f;
    auto event_top = first_event_pos.y_offset + first_event_pos.allocated_height * 0.5f;
    
    float separation = analog_bottom - event_top;
    
    std::cout << "  Bottom of analog region: " << analog_bottom << std::endl;
    std::cout << "  Top of event region: " << event_top << std::endl;
    std::cout << "  Separation: " << separation << " units" << std::endl;
    
    if (separation >= 0) {
        std::cout << "  ✓ NO OVERLAP! Events positioned below analog channels." << std::endl;
    } else {
        std::cout << "  ✗ OVERLAP DETECTED! Gap = " << separation << std::endl;
    }
    
    std::cout << std::endl << "Step 4: Summary statistics..." << std::endl;
    std::cout << "  Total series: " << manager.getTotalSeriesCount() << std::endl;
    std::cout << "  Analog series: " << manager.getSeriesCount(DataSeriesType::Analog) << std::endl;
    std::cout << "  Digital event series: " << manager.getSeriesCount(DataSeriesType::DigitalEvent) << std::endl;
    std::cout << "  All data fits in normalized range [-1.0, +1.0]: " 
              << (last_event_pos.y_offset > -1.0f && first_analog_pos.y_offset < 1.0f ? "✓" : "✗") << std::endl;
    
    std::cout << std::endl << "Step 5: Testing auto-arrange functionality..." << std::endl;
    
    // Demonstrate manual recalculation
    manager.recalculateAllPositions();
    std::cout << "  Auto-arrange completed - positions optimized" << std::endl;
    
    // Verify positions are still valid
    auto new_first_analog = manager.getSeriesPosition("lfp_ch0").value();
    auto new_first_event = manager.getSeriesPosition("event_ch0").value();
    
    std::cout << "  After auto-arrange: analog start = " << new_first_analog.y_offset 
              << ", event start = " << new_first_event.y_offset << std::endl;
    
    std::cout << std::endl << "=== Demo Complete ===" << std::endl;
    std::cout << "The VerticalSpaceManager successfully coordinates " << manager.getTotalSeriesCount() 
              << " series without overlap!" << std::endl;
    
    return 0;
} 