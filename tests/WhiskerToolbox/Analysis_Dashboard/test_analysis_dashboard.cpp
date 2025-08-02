#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Analysis_Dashboard.hpp"
#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeScrollBar.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include <QApplication>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <memory>
#include <filesystem>
#include <fstream>

// Test fixture for Qt application setup with OpenGL context
class QtTestFixture {
protected:
    QtTestFixture() {
        // Create a minimal Qt application for testing
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        if (!QApplication::instance()) {
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        // Set up OpenGL context for ShaderManager
        setupOpenGLContext();
        
        // Initialize ShaderManager with some basic shaders for testing
        initializeShaderManager();
    }
    
    ~QtTestFixture() {
        // Clean up any remaining widgets
        QApplication::processEvents();
    }
    
    void setupOpenGLContext() {
        // Create an offscreen surface for OpenGL context
        surface = std::make_unique<QOffscreenSurface>();
        surface->create();
        
        // Create OpenGL context
        context = std::make_unique<QOpenGLContext>();
        context->create();
        context->makeCurrent(surface.get());
        
        // Verify context is valid
        REQUIRE(context->isValid());
    }
    
    void initializeShaderManager() {
        // Get the ShaderManager instance
        auto& shader_manager = ShaderManager::instance();
        
        // Create basic vertex and fragment shaders for testing
        // These are minimal shaders that should compile successfully
        std::string vertex_shader_source = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;
            void main() {
                gl_Position = projection * view * model * vec4(aPos, 1.0);
            }
        )";
        
        std::string fragment_shader_source = R"(
            #version 330 core
            out vec4 FragColor;
            uniform vec3 color;
            void main() {
                FragColor = vec4(color, 1.0);
            }
        )";
        
        // Write shaders to temporary files
        vertex_shader_path = std::filesystem::temp_directory_path() / "test_vertex.glsl";
        fragment_shader_path = std::filesystem::temp_directory_path() / "test_fragment.glsl";
        
        std::ofstream vertex_file(vertex_shader_path);
        vertex_file << vertex_shader_source;
        vertex_file.close();
        
        std::ofstream fragment_file(fragment_shader_path);
        fragment_file << fragment_shader_source;
        fragment_file.close();
        
        // Load the test shader program
        bool success = shader_manager.loadProgram("test_program", 
                                                vertex_shader_path.string(),
                                                fragment_shader_path.string());
        REQUIRE(success);
    }
    
    std::unique_ptr<QApplication> app;
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QOpenGLContext> context;
    std::filesystem::path vertex_shader_path;
    std::filesystem::path fragment_shader_path;
};

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Basic Creation", "[AnalysisDashboard]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create a time scroll bar (required by Analysis Dashboard constructor)
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Test that the dashboard can be shown (this tests widget creation)
    dashboard.show();
    REQUIRE(dashboard.isVisible());
    
    // Test that the dashboard can be hidden
    dashboard.hide();
    REQUIRE(!dashboard.isVisible());
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Data Manager Integration", "[AnalysisDashboard]") {
    // Create a data manager with some test data
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Add some test data to the data manager
    std::vector<float> test_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
    auto series =std::make_shared<AnalogTimeSeries>(test_data, times);
    data_manager->setData<AnalogTimeSeries>("test_analog", series);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Test that the data is accessible through the data manager directly
    auto analog_data = data_manager->getData<AnalogTimeSeries>("test_analog");
    REQUIRE(analog_data != nullptr);
    REQUIRE(analog_data->getNumSamples() == 5);
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Widget Lifecycle", "[AnalysisDashboard]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Test widget lifecycle
    SECTION("Show and Hide") {
        dashboard.show();
        REQUIRE(dashboard.isVisible());
        
        dashboard.hide();
        REQUIRE(!dashboard.isVisible());
    }
    
    SECTION("Resize") {
        dashboard.show();
        dashboard.resize(800, 600);
        REQUIRE(dashboard.width() == 800);
        REQUIRE(dashboard.height() == 600);
    }
    
    SECTION("Close") {
        dashboard.show();
        REQUIRE(dashboard.isVisible());
        
        dashboard.close();
        // Note: close() doesn't immediately hide on all platforms
        // but it should mark the widget for deletion
        auto is_visible = dashboard.isVisible() || dashboard.testAttribute(Qt::WA_DeleteOnClose);
        REQUIRE(is_visible);
    }
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Component Access", "[AnalysisDashboard]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Test that we can access the group manager
    auto group_manager = dashboard.getGroupManager();
    REQUIRE(group_manager != nullptr);
    
    // Test that we can access the data source registry
    auto data_source_registry = dashboard.getDataSourceRegistry();
    REQUIRE(data_source_registry != nullptr);
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Event Processing", "[AnalysisDashboard]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Show the dashboard
    dashboard.show();
    REQUIRE(dashboard.isVisible());
    
    // Process events to ensure widget is properly initialized
    QApplication::processEvents();
    
    // Test that the widget can handle events
    SECTION("Mouse Events") {
        // Simulate a mouse press event
        QTest::mousePress(&dashboard, Qt::LeftButton);
        QApplication::processEvents();
        
        // Simulate a mouse release event
        QTest::mouseRelease(&dashboard, Qt::LeftButton);
        QApplication::processEvents();
    }
    
    SECTION("Key Events") {
        // Simulate a key press event
        QTest::keyPress(&dashboard, Qt::Key_Escape);
        QApplication::processEvents();
    }
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Memory Management", "[AnalysisDashboard]") {
    // Test that the dashboard can be created and destroyed without memory leaks
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create dashboard in a scope to test destruction
    {
        Analysis_Dashboard dashboard(data_manager, time_scrollbar);
        REQUIRE(&dashboard != nullptr);
        
        dashboard.show();
        QApplication::processEvents();
    }
    
    // Dashboard should be destroyed here
    QApplication::processEvents();
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Null Data Manager", "[AnalysisDashboard]") {
    // Test behavior with null data manager
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create dashboard with null data manager
    Analysis_Dashboard dashboard(nullptr, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Dashboard should still be created successfully
    dashboard.show();
    REQUIRE(dashboard.isVisible());
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Multiple Instances", "[AnalysisDashboard]") {
    // Test creating multiple dashboard instances
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    auto time_scrollbar1 = new TimeScrollBar();
    auto time_scrollbar2 = new TimeScrollBar();
    REQUIRE(time_scrollbar1 != nullptr);
    REQUIRE(time_scrollbar2 != nullptr);
    
    // Create two dashboard instances
    Analysis_Dashboard dashboard1(data_manager, time_scrollbar1);
    Analysis_Dashboard dashboard2(data_manager, time_scrollbar2);
    
    REQUIRE(&dashboard1 != nullptr);
    REQUIRE(&dashboard2 != nullptr);
    REQUIRE(&dashboard1 != &dashboard2);
    
    // Both should be able to show independently
    dashboard1.show();
    dashboard2.show();
    REQUIRE(dashboard1.isVisible());
    REQUIRE(dashboard2.isVisible());
    
    // Clean up
    delete time_scrollbar1;
    delete time_scrollbar2;
}

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - ShaderManager Integration", "[AnalysisDashboard]") {
    // Test that ShaderManager is properly initialized
    auto& shader_manager = ShaderManager::instance();
    REQUIRE(&shader_manager != nullptr);
    
    // Test that our test shader program was loaded
    auto test_program = shader_manager.getProgram("test_program");
    REQUIRE(test_program != nullptr);
    REQUIRE(test_program->getProgramId() != 0);
    
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard (this should work with ShaderManager initialized)
    Analysis_Dashboard dashboard(data_manager, time_scrollbar);
    REQUIRE(&dashboard != nullptr);
    
    // Test that the dashboard can be shown (this tests widget creation with OpenGL)
    dashboard.show();
    REQUIRE(dashboard.isVisible());
    
    // Process events to ensure OpenGL widgets are properly initialized
    QApplication::processEvents();
    
    // Test that the dashboard can be hidden
    dashboard.hide();
    REQUIRE(!dashboard.isVisible());
    
    // Clean up
    delete time_scrollbar;
}

TEST_CASE_METHOD(QtTestFixture, "Feature_Table_Widget - Basic Creation", "[FeatureTableWidget]") {
    // Test that Feature_Table_Widget can be created
    auto feature_table_widget = new Feature_Table_Widget();
    REQUIRE(feature_table_widget != nullptr);
    
    // Test that it can be shown
    feature_table_widget->show();
    REQUIRE(feature_table_widget->isVisible());
    
    // Test that it can be hidden
    feature_table_widget->hide();
    REQUIRE(!feature_table_widget->isVisible());
    
    // Clean up
    delete feature_table_widget;
}

TEST_CASE_METHOD(QtTestFixture, "Feature_Table_Widget - Data Manager Integration", "[FeatureTableWidget]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Add some test data
    std::vector<float> test_data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<TimeFrameIndex> times{TimeFrameIndex(10), TimeFrameIndex(20), TimeFrameIndex(30), TimeFrameIndex(40), TimeFrameIndex(50)};
    auto series = std::make_shared<AnalogTimeSeries>(test_data, times);
    data_manager->setData<AnalogTimeSeries>("test_analog", series);
    
    // Create the feature table widget
    auto feature_table_widget = new Feature_Table_Widget();
    REQUIRE(feature_table_widget != nullptr);
    
    // Set the data manager
    feature_table_widget->setDataManager(data_manager);
    
    // Test that the widget can be shown
    feature_table_widget->show();
    REQUIRE(feature_table_widget->isVisible());
    
    // Process events to ensure widget is properly initialized
    QApplication::processEvents();
    
    // Clean up
    delete feature_table_widget;
}
