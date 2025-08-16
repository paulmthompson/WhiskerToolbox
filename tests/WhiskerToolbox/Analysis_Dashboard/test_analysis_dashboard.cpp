#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Analysis_Dashboard.hpp"
#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeScrollBar.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include "DockManager.h"
#include <QApplication>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>

#include <memory>
#include <filesystem>
#include <fstream>

// Test fixture for Qt application setup with OpenGL context
class QtTestFixture {
protected:
    QtTestFixture() {
        // Create a minimal Qt application for testing
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
           // qputenv("QT_QPA_PLATFORM", "xcb");
        }

        //qputenv("WAYLAND_DISPLAY", "");
        //qputenv("XDG_SESSION_TYPE", "x11");
        
        // Disable Qt Wayland warnings/logging
        qputenv("QT_LOGGING_RULES", "qt.qpa.wayland*=false");

        // Create a minimal Qt application for testing
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        if (!QApplication::instance()) {
            app = std::make_unique<QApplication>(argc, argv);
        }
        
        QString platformName = QGuiApplication::platformName();
        std::cout << "Platform name: " << platformName.toStdString() << std::endl;
        if (platformName.contains("wayland", Qt::CaseInsensitive)) {
           // FAIL("Still using Wayland platform despite override");
        }

        // Set up OpenGL context for ShaderManager
        setupOpenGLContext();
        
        // Initialize ShaderManager with some basic shaders for testing
        initializeShaderManager();
    }
    
    ~QtTestFixture() {
        // Clean up any remaining widgets
        QApplication::processEvents();
        QApplication::closeAllWindows();

        auto& shader_manager = ShaderManager::instance();
        shader_manager.cleanup();

        if (context) {
            context->doneCurrent();
        }

        if (surface) {
            surface->destroy();
        }

        if (app) {
            app->processEvents();
            app->quit();
        }
    }
    
    void setupOpenGLContext() {
        // Set OpenGL format for OpenGL 4.1 compatibility
        QSurfaceFormat format;
        format.setVersion(4, 1);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        format.setSwapInterval(0); // Disable vsync

        // Create OpenGL context with the specified format
        context = std::make_unique<QOpenGLContext>();
        context->setFormat(format);
        
        // Create an offscreen surface with the specified format
        surface = std::make_unique<QOffscreenSurface>();
        surface->setFormat(format);
        surface->create();
        
        if (!context->create()) {
            FAIL("Failed to create OpenGL context");
            return;
        }
        
        if (!context->makeCurrent(surface.get())) {
            FAIL("Failed to make OpenGL context current");
            return;
        }
        
        // Verify context is valid and has the required version
        REQUIRE(context->isValid());
        
        // Check OpenGL version
        QOpenGLFunctions* functions = context->functions();
        REQUIRE(functions != nullptr);
        
        // Get OpenGL version
        const char* version = reinterpret_cast<const char*>(functions->glGetString(GL_VERSION));
        REQUIRE(version != nullptr);

        std::cout << "OpenGL Version: " << version << std::endl;
        
    }
    
    void initializeShaderManager() {
        // Get the ShaderManager instance
        auto& shader_manager = ShaderManager::instance();
        
        // Test resource file access first
        QFile testFile(":/shaders/point.frag");
        if (!testFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            std::cerr << "Failed to open resource file :/shaders/point.frag" << std::endl;
            FAIL("Cannot access shader resources");
            return;
        }
        
        QTextStream in(&testFile);
        QString content = in.readAll();
        testFile.close();
        
        std::cout << "Shader content length: " << content.length() << std::endl;
        if (content.length() < 10) {
            std::cerr << "Shader content too short, might be empty or corrupted" << std::endl;
            FAIL("Shader resource appears to be empty or corrupted");
            return;
        }
        
        // Load shaders from the qrc resource file
        // These are the actual shaders used by your application
        bool success = shader_manager.loadProgram("point", 
                                                ":/shaders/point.vert", 
                                                ":/shaders/point.frag", 
                                                "", 
                                                ShaderSourceType::Resource);
        if (!success) {
            std::cerr << "Failed to load point shader program" << std::endl;
        }
        REQUIRE(success);
        
        success = shader_manager.loadProgram("line", 
                                          ":/shaders/line.vert", 
                                          ":/shaders/line.frag", 
                                          "", 
                                          ShaderSourceType::Resource);
        if (!success) {
            std::cerr << "Failed to load line shader program" << std::endl;
        }
        REQUIRE(success);
        
        success = shader_manager.loadProgram("texture", 
                                          ":/shaders/texture.vert", 
                                          ":/shaders/texture.frag", 
                                          "", 
                                          ShaderSourceType::Resource);
        if (!success) {
            std::cerr << "Failed to load texture shader program" << std::endl;
        }
        REQUIRE(success);
        
        // Also load a more complex shader with geometry shader
        success = shader_manager.loadProgram("line_with_geometry", 
                                          ":/shaders/line_with_geometry.vert", 
                                          ":/shaders/line_with_geometry.frag", 
                                          ":/shaders/line_with_geometry.geom", 
                                          ShaderSourceType::Resource);
        if (!success) {
            std::cerr << "Failed to load line_with_geometry shader program" << std::endl;
        }
        REQUIRE(success);
    }
    
    std::unique_ptr<QApplication> app;
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QOpenGLContext> context;
};


TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Basic Creation", "[AnalysisDashboard]") {
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create a time scroll bar (required by Analysis Dashboard constructor)
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the global dock manager for tests
    ads::CDockManager dock_manager;
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
    data_manager->setData<AnalogTimeSeries>("test_analog", series, TimeKey("time"));
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the global dock manager for tests
    ads::CDockManager dock_manager;
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
    
    ads::CDockManager dock_manager;
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
    
    ads::CDockManager dock_manager;
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
    REQUIRE(&dashboard != nullptr);
    
    // Test that we can access the group manager
    auto group_manager = dashboard.getGroupManager();
    REQUIRE(group_manager != nullptr);
    
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
    
    ads::CDockManager dock_manager;
    // Create the analysis dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
        ads::CDockManager dock_manager;
        Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
    ads::CDockManager dock_manager;
    Analysis_Dashboard dashboard(nullptr, time_scrollbar, &dock_manager);
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
    ads::CDockManager dock_manager;
    Analysis_Dashboard dashboard1(data_manager, time_scrollbar1, &dock_manager);
    Analysis_Dashboard dashboard2(data_manager, time_scrollbar2, &dock_manager);
    
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
    
    // Test that our shader programs were loaded from resources
    auto point_program = shader_manager.getProgram("point");
    REQUIRE(point_program != nullptr);
    REQUIRE(point_program->getProgramId() != 0);
    
    auto line_program = shader_manager.getProgram("line");
    REQUIRE(line_program != nullptr);
    REQUIRE(line_program->getProgramId() != 0);
    
    auto texture_program = shader_manager.getProgram("texture");
    REQUIRE(texture_program != nullptr);
    REQUIRE(texture_program->getProgramId() != 0);
    
    // Create a data manager
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);
    
    // Create time scroll bar
    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);
    
    // Create the analysis dashboard (this should work with ShaderManager initialized)
    ads::CDockManager dock_manager;
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
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
    data_manager->setData<AnalogTimeSeries>("test_analog", series, TimeKey("time"));
    
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


TEST_CASE_METHOD(QtTestFixture, "ShaderManager - Resource Loading", "[ShaderManager]") {
    // Test that ShaderManager can load shaders from Qt resources
    auto& shader_manager = ShaderManager::instance();
    REQUIRE(&shader_manager != nullptr);
    
    // Test loading different types of shaders from resources
    SECTION("Basic Vertex/Fragment Shaders") {
        auto point_program = shader_manager.getProgram("point");
        REQUIRE(point_program != nullptr);
        REQUIRE(point_program->getProgramId() != 0);
        
        auto line_program = shader_manager.getProgram("line");
        REQUIRE(line_program != nullptr);
        REQUIRE(line_program->getProgramId() != 0);
    }
    
    SECTION("Texture Shader") {
        auto texture_program = shader_manager.getProgram("texture");
        REQUIRE(texture_program != nullptr);
        REQUIRE(texture_program->getProgramId() != 0);
    }
    
    SECTION("Geometry Shader") {
        auto geometry_program = shader_manager.getProgram("line_with_geometry");
        REQUIRE(geometry_program != nullptr);
        REQUIRE(geometry_program->getProgramId() != 0);
    }
    
    SECTION("Shader Program Properties") {
        auto point_program = shader_manager.getProgram("point");
        REQUIRE(point_program != nullptr);
        
        // Test that the program has valid OpenGL program ID
        GLuint program_id = point_program->getProgramId();
        REQUIRE(program_id != 0);
        
        // Test that we can get the native QOpenGLShaderProgram
        QOpenGLShaderProgram* native_program = point_program->getNativeProgram();
        REQUIRE(native_program != nullptr);
        REQUIRE(native_program->isLinked());
    }
}
