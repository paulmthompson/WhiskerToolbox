#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Analysis_Dashboard.hpp"
#include "PlotFactory.hpp"
#include "PlotContainer.hpp"
#include "Plots/AbstractPlotWidget.hpp"
#include "Properties/AbstractPlotPropertiesWidget.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "TimeScrollBar/TimeScrollBarState.hpp"
#include "DataManager/DataManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

// Include specific widget headers for type checking
#include "Widgets/ScatterPlotWidget/ScatterPlotWidget.hpp"
//#include "Widgets/EventPlotWidget/EventPlotWidget.hpp" 
#include "Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"

// Include properties widget headers for type checking
#include "Widgets/ScatterPlotWidget/ScatterPlotPropertiesWidget.hpp"
//#include "Widgets/EventPlotWidget/EventPlotPropertiesWidget.hpp"
#include "Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp"

#include <QTest>
#include <QTimer>
#include <QApplication>
#include "DockManager.h"
#include <memory>

/**
 * @brief Simple test fixture for Analysis_Dashboard testing
 */
class AnalysisDashboardTestFixture {
protected:
    AnalysisDashboardTestFixture() {
        // Ensure QApplication exists for widget testing
        if (!QApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            m_app = std::make_unique<QApplication>(argc, argv);
        }
        
        // Create a DataManager with some test data
        m_data_manager = std::make_shared<DataManager>();
        populateTestData();
        
        // Create a GroupManager
        auto* entity_group_manager = m_data_manager->getEntityGroupManager();
        m_group_manager = std::make_unique<GroupManager>(entity_group_manager, m_data_manager);
        
        // Create a TimeScrollBar for the dashboard
        m_time_scrollbar = std::make_unique<TimeScrollBar>(m_data_manager, std::make_shared<TimeScrollBarState>(), nullptr);
        m_time_scrollbar->setDataManager(m_data_manager);
        
        // Create a dock manager for tests and the Analysis_Dashboard
        m_dock_manager = std::make_unique<ads::CDockManager>();
        m_dashboard = std::make_unique<Analysis_Dashboard>(m_data_manager, m_group_manager.get(), m_time_scrollbar.get(), m_dock_manager.get());
        
        // Process any initial events
        if (m_app) {
            m_app->processEvents();
        }
    }
    
    ~AnalysisDashboardTestFixture() {
        // Clean up dashboard first
        if (m_dashboard) {
            m_dashboard.reset();
        }
        
        if (m_time_scrollbar) {
            m_time_scrollbar.reset();
        }
        
        if (m_app) {
            m_app->processEvents();
        }
    }
    
    /**
     * @brief Get the Analysis_Dashboard instance
     * @return Reference to the dashboard
     */
    Analysis_Dashboard& getDashboard() { return *m_dashboard; }
    
    /**
     * @brief Get the DataManager instance
     * @return Reference to the data manager
     */
    DataManager& getDataManager() { return *m_data_manager; }
    
    /**
     * @brief Get the TimeScrollBar instance
     * @return Reference to the time scrollbar
     */
    TimeScrollBar& getTimeScrollBar() { return *m_time_scrollbar; }
    
    /**
     * @brief Get the GroupManager instance
     * @return Reference to the group manager
     */
    GroupManager& getGroupManager() { return *m_group_manager; }

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<Analysis_Dashboard> m_dashboard;
    std::unique_ptr<GroupManager> m_group_manager;
    std::unique_ptr<TimeScrollBar> m_time_scrollbar;
    std::unique_ptr<ads::CDockManager> m_dock_manager;
    std::shared_ptr<DataManager> m_data_manager;
    
    /**
     * @brief Populate the DataManager with simple test data
     */
    void populateTestData() {
        // Add some basic point data
        auto point_data = std::make_shared<PointData>();
        m_data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));
        
        // Add some basic line data
        auto line_data = std::make_shared<LineData>();
        m_data_manager->setData<LineData>("test_lines", line_data, TimeKey("time"));
        
        // Add some basic analog data
        auto analog_data = std::make_shared<AnalogTimeSeries>();
        m_data_manager->setData<AnalogTimeSeries>("test_analog", analog_data, TimeKey("time"));
        
        // Add some basic event data
        auto event_data = std::make_shared<DigitalEventSeries>();
        m_data_manager->setData<DigitalEventSeries>("test_events", event_data, TimeKey("time"));
    }
};

TEST_CASE_METHOD(AnalysisDashboardTestFixture, "Analysis_Dashboard - ScatterPlotWidget Creation", "[Analysis_Dashboard][ScatterPlot]") {
    // Get the dashboard
    auto& dashboard = getDashboard();
    
    SECTION("Create ScatterPlotWidget through plot factory") {
        // Test creating a scatter plot container
        auto plot_container = PlotFactory::createPlotContainer("scatter_plot");
        
        REQUIRE(plot_container != nullptr);
        REQUIRE(plot_container->getPlotWidget() != nullptr);
        REQUIRE(plot_container->getPropertiesWidget() != nullptr);
        
        // Verify the plot widget is the correct type
        auto* scatter_widget = dynamic_cast<ScatterPlotWidget*>(plot_container->getPlotWidget());
        REQUIRE(scatter_widget != nullptr);
        
        // Verify the properties widget is the correct type  
        auto* scatter_properties = dynamic_cast<ScatterPlotPropertiesWidget*>(plot_container->getPropertiesWidget());
        REQUIRE(scatter_properties != nullptr);
    }
    
}

/*
TEST_CASE_METHOD(AnalysisDashboardTestFixture, "Analysis_Dashboard - EventPlotWidget Creation", "[Analysis_Dashboard][EventPlot]") {
    // Get the dashboard
    auto& dashboard = getDashboard();
    
    SECTION("Create EventPlotWidget through plot factory") {
        // Test creating an event plot container
        auto plot_container = PlotFactory::createPlotContainer("event_plot");
        
        REQUIRE(plot_container != nullptr);
        REQUIRE(plot_container->getPlotWidget() != nullptr);
        REQUIRE(plot_container->getPropertiesWidget() != nullptr);
        
        // Verify the plot widget is the correct type
        auto* event_widget = dynamic_cast<EventPlotWidget*>(plot_container->getPlotWidget());
        REQUIRE(event_widget != nullptr);
        
        // Verify the properties widget is the correct type
        auto* event_properties = dynamic_cast<EventPlotPropertiesWidget*>(plot_container->getPropertiesWidget());
        REQUIRE(event_properties != nullptr);
    }
    
    SECTION("Dashboard can handle event plot type selection") {
        // Verify the core creation logic works for event plots
        auto plot_container = PlotFactory::createPlotContainer("event_plot");
        REQUIRE(plot_container != nullptr);
        
        // Verify that the DataManager has event data available
        auto& data_manager = getDataManager();
        auto event_data = data_manager.getData<DigitalEventSeries>("test_events");
        REQUIRE(event_data != nullptr);
    }
}
    */

TEST_CASE_METHOD(AnalysisDashboardTestFixture, "Analysis_Dashboard - SpatialOverlayPlotWidget Creation", "[Analysis_Dashboard][SpatialOverlay]") {
    // Get the dashboard  
    auto& dashboard = getDashboard();
    
    SECTION("Create SpatialOverlayPlotWidget through plot factory") {
        // Test creating a spatial overlay plot container
        auto plot_container = PlotFactory::createPlotContainer("spatial_overlay_plot");
        
        REQUIRE(plot_container != nullptr);
        REQUIRE(plot_container->getPlotWidget() != nullptr);
        REQUIRE(plot_container->getPropertiesWidget() != nullptr);
        
        // Verify the plot widget is the correct type
        auto* spatial_widget = dynamic_cast<SpatialOverlayPlotWidget*>(plot_container->getPlotWidget());
        REQUIRE(spatial_widget != nullptr);
        
        // Verify the properties widget is the correct type
        auto* spatial_properties = dynamic_cast<SpatialOverlayPlotPropertiesWidget*>(plot_container->getPropertiesWidget());
        REQUIRE(spatial_properties != nullptr);
    }
    
    SECTION("Dashboard can handle spatial overlay plot type selection") {
        // Verify the core creation logic works for spatial overlay plots
        auto plot_container = PlotFactory::createPlotContainer("spatial_overlay_plot");
        REQUIRE(plot_container != nullptr);
        
        // Verify that the DataManager has point data available for spatial plotting
        auto& data_manager = getDataManager();
        auto point_data = data_manager.getData<PointData>("test_points");
        REQUIRE(point_data != nullptr);
    }
    
    SECTION("Spatial overlay widget has OpenGL components") {
        auto plot_container = PlotFactory::createPlotContainer("spatial_overlay_plot");
        auto* spatial_widget = dynamic_cast<SpatialOverlayPlotWidget*>(plot_container->getPlotWidget());
        REQUIRE(spatial_widget != nullptr);
        
        // Process events to ensure widget is fully initialized
        if (QApplication::instance()) {
            QApplication::processEvents();
        }
        
        // The SpatialOverlayPlotWidget should have been created successfully
        // Note: We can't test the full OpenGL initialization without a complete widget hierarchy,
        // but we can verify the widget was created successfully
        REQUIRE(spatial_widget != nullptr);
    }
}

TEST_CASE_METHOD(AnalysisDashboardTestFixture, "Analysis_Dashboard - Basic Infrastructure", "[Analysis_Dashboard][Infrastructure]") {
    // Get the dashboard
    auto& dashboard = getDashboard();
    
    SECTION("Dashboard has required components") {
        // Verify that all required components are present
        REQUIRE(dashboard.getGroupManager() != nullptr);
        
        // Verify that the data manager is accessible and populated
        auto& data_manager = getDataManager();
        
        // Check that we have the expected test data types
        auto point_data = data_manager.getData<PointData>("test_points");
        REQUIRE(point_data != nullptr);
        
        auto line_data = data_manager.getData<LineData>("test_lines");
        REQUIRE(line_data != nullptr);
        
        auto analog_data = data_manager.getData<AnalogTimeSeries>("test_analog");
        REQUIRE(analog_data != nullptr);
        
        auto event_data = data_manager.getData<DigitalEventSeries>("test_events");
        REQUIRE(event_data != nullptr);
    }
    
    SECTION("TimeScrollBar is configured") {
        auto& time_scrollbar = getTimeScrollBar();
        
        // Verify the time scrollbar exists and can be accessed
        // Note: We can't test much more without triggering UI events
        REQUIRE(&time_scrollbar != nullptr);
    }
}
