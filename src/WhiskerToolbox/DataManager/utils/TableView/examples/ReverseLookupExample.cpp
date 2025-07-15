#include "utils/TableView/core/TableView.h"
#include "utils/TableView/core/TableViewBuilder.h"
#include "utils/TableView/core/RowDescriptor.h"
#include "utils/TableView/interfaces/IRowSelector.h"
#include "DigitalTimeSeries/interval_data.hpp"
#include "TimeFrame.hpp"

#include <iostream>
#include <variant>
#include <string>

/**
 * @brief Example demonstrating the reverse lookup feature for interactive plotting.
 * 
 * This example shows how a plotting library would use the TableView reverse lookup
 * API to create tooltips that trace back to the original data source.
 */

// This function would be called by the plotting library on mouse hover.
void on_plot_hover(const TableView& table, size_t hovered_row_index) {
    std::cout << "Mouse hover detected on row " << hovered_row_index << std::endl;
    
    // 1. Get the generic descriptor for the hovered row.
    RowDescriptor desc = table.getRowDescriptor(hovered_row_index);

    // 2. Use std::visit with a lambda to process the descriptor type-safely.
    std::string tooltip_text = std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, size_t>) {
            return "Source Index: " + std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return "Source Timestamp: " + std::to_string(arg);
        } else if constexpr (std::is_same_v<T, TimeFrameInterval>) {
            return "Source Interval: [" + std::to_string(arg.start.getValue()) + 
                   ", " + std::to_string(arg.end.getValue()) + "]";
        } else {
            return "Source: N/A";
        }
    }, desc);

    // 3. Display the generated tooltip text to the user.
    std::cout << "Tooltip: " << tooltip_text << std::endl;
}

void demonstrate_reverse_lookup() {
    std::cout << "=== TableView Reverse Lookup Demo ===" << std::endl;
    
    // Example 1: IndexSelector
    std::cout << "\n1. IndexSelector Example:" << std::endl;
    std::vector<size_t> indices = {5, 10, 15, 20};
    auto indexSelector = std::make_unique<IndexSelector>(indices);
    
    // Simulate a table with IndexSelector (simplified for demo)
    std::cout << "   Table rows based on indices: [5, 10, 15, 20]" << std::endl;
    for (size_t i = 0; i < indices.size(); ++i) {
        RowDescriptor desc = indexSelector->getDescriptor(i);
        std::cout << "   Row " << i << " -> ";
        if (std::holds_alternative<size_t>(desc)) {
            std::cout << "Index " << std::get<size_t>(desc) << std::endl;
        }
    }
    
    // Example 2: TimestampSelector
    std::cout << "\n2. TimestampSelector Example:" << std::endl;
    std::vector<double> timestamps = {1.5, 2.7, 3.1, 4.8};
    auto timestampSelector = std::make_unique<TimestampSelector>(timestamps);
    
    std::cout << "   Table rows based on timestamps: [1.5, 2.7, 3.1, 4.8]" << std::endl;
    for (size_t i = 0; i < timestamps.size(); ++i) {
        RowDescriptor desc = timestampSelector->getDescriptor(i);
        std::cout << "   Row " << i << " -> ";
        if (std::holds_alternative<double>(desc)) {
            std::cout << "Timestamp " << std::get<double>(desc) << std::endl;
        }
    }
    
    // Example 3: IntervalSelector
    std::cout << "\n3. IntervalSelector Example:" << std::endl;
    std::vector<TimeFrameInterval> intervals = {
        {TimeFrameIndex(0), TimeFrameIndex(10)},
        {TimeFrameIndex(15), TimeFrameIndex(25)},
        {TimeFrameIndex(30), TimeFrameIndex(40)}
    };
    auto intervalSelector = std::make_unique<IntervalSelector>(intervals);
    
    std::cout << "   Table rows based on intervals: [[0,10], [15,25], [30,40]]" << std::endl;
    for (size_t i = 0; i < intervals.size(); ++i) {
        RowDescriptor desc = intervalSelector->getDescriptor(i);
        std::cout << "   Row " << i << " -> ";
        if (std::holds_alternative<TimeFrameInterval>(desc)) {
            auto interval = std::get<TimeFrameInterval>(desc);
            std::cout << "Interval [" << interval.start.getValue() << ", " 
                     << interval.end.getValue() << "]" << std::endl;
        }
    }
    
    // Example 4: Simulated plotting interaction
    std::cout << "\n4. Simulated Plotting Interaction (using IndexSelector):" << std::endl;
    
    // Create a mock TableView with the IndexSelector
    // In a real scenario, this would be a fully constructed TableView
    class MockTableView {
        std::unique_ptr<IRowSelector> m_selector;
    public:
        MockTableView(std::unique_ptr<IRowSelector> selector) : m_selector(std::move(selector)) {}
        
        RowDescriptor getRowDescriptor(size_t row_index) const {
            return m_selector->getDescriptor(row_index);
        }
    };
    
    MockTableView mockTable(std::make_unique<IndexSelector>(std::vector<size_t>{100, 200, 300}));
    
    // Simulate user hovering over different rows
    for (size_t row = 0; row < 3; ++row) {
        on_plot_hover(mockTable, row);
    }
    
    std::cout << "\n=== Demo Complete ===" << std::endl;
}

// For compilation purposes, provide a simple main function
int main() {
    demonstrate_reverse_lookup();
    return 0;
}
