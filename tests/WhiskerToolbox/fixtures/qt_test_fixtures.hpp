#ifndef QT_TEST_FIXTURES_HPP
#define QT_TEST_FIXTURES_HPP

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <QApplication>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <QGuiApplication>

#include <memory>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <tuple>

// Forward declarations
class ShaderManager;
class GroupManager;
class PointData;
class BoundingBox;
class TimeFrameIndex;

// Include necessary headers for the test fixtures
#include "ShaderManager/ShaderManager.hpp"
#include "ShaderManager/ShaderSourceType.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "CoreGeometry/points.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "SpatialIndex/QuadTree.hpp"
#include "CoreGeometry/boundingbox.hpp"

/**
 * @brief Test fixture for Qt application setup with OpenGL 4.3 context
 * 
 * This fixture provides a complete testing environment for Qt widgets that require
 * OpenGL rendering, including proper context setup and ShaderManager initialization.
 */
class QtOpenGLTestFixture {
protected:
    QtOpenGLTestFixture() {
        // Create a minimal Qt application for testing
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            // qputenv("QT_QPA_PLATFORM", "xcb");
        }

        // qputenv("WAYLAND_DISPLAY", "");
        // qputenv("XDG_SESSION_TYPE", "x11");
        
        // Disable Qt Wayland warnings/logging
        qputenv("QT_LOGGING_RULES", "qt.qpa.wayland*=false");

        // Create a minimal Qt application for testing
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        if (!QApplication::instance()) {
            m_app = std::make_unique<QApplication>(argc, argv);
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
    
    ~QtOpenGLTestFixture() {
        // Clean up any remaining widgets
        QApplication::processEvents();
        QApplication::closeAllWindows();

        auto& shader_manager = ShaderManager::instance();
        shader_manager.cleanup();

        if (m_context) {
            m_context->doneCurrent();
        }

        if (m_surface) {
            m_surface->destroy();
        }

        if (m_app) {
            m_app->processEvents();
            m_app->quit();
        }
    }
    

    
    /**
     * @brief Set up OpenGL 4.1 context with offscreen surface
     */
    void setupOpenGLContext() {
        // Set OpenGL format for OpenGL 4.1 compatibility (same as working integration test)
        QSurfaceFormat format;
        format.setVersion(4, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        format.setSwapInterval(0); // Disable vsync

        // Create OpenGL context with the specified format
        m_context = std::make_unique<QOpenGLContext>();
        m_context->setFormat(format);
        
        // Create an offscreen surface with the specified format
        m_surface = std::make_unique<QOffscreenSurface>();
        m_surface->setFormat(format);
        m_surface->create();
        
        if (!m_context->create()) {
            FAIL("Failed to create OpenGL context");
            return;
        }
        
        if (!m_context->makeCurrent(m_surface.get())) {
            FAIL("Failed to make OpenGL context current");
            return;
        }
        
        // Verify context is valid and has the required version
        REQUIRE(m_context->isValid());
        
        // Check OpenGL version
        QOpenGLFunctions* functions = m_context->functions();
        REQUIRE(functions != nullptr);
        
        // Get OpenGL version
        const char* version = reinterpret_cast<const char*>(functions->glGetString(GL_VERSION));
        REQUIRE(version != nullptr);

        std::cout << "OpenGL Version: " << version << std::endl;
 
    }
    
    /**
     * @brief Initialize ShaderManager with test shaders
     */
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
    

    
    /**
     * @brief Create test PointData with sample data
     * @return Shared pointer to PointData with test data
     */
    std::shared_ptr<PointData> createTestPointData() {
        auto point_data = std::make_shared<PointData>();
        
        // Add test points at different time frames
        std::vector<Point2D<float>> points_frame_1 = {
            {10.0f, 20.0f},
            {15.0f, 25.0f},
            {20.0f, 30.0f}
        };
        
        std::vector<Point2D<float>> points_frame_2 = {
            {30.0f, 40.0f},
            {35.0f, 45.0f}
        };
        
        std::vector<Point2D<float>> points_frame_3 = {
            {50.0f, 60.0f},
            {55.0f, 65.0f},
            {60.0f, 70.0f},
            {65.0f, 75.0f}
        };
        
        point_data->overwritePointsAtTime(TimeFrameIndex(1), points_frame_1);
        point_data->overwritePointsAtTime(TimeFrameIndex(2), points_frame_2);
        point_data->overwritePointsAtTime(TimeFrameIndex(3), points_frame_3);
        
        return point_data;
    }
    
    /**
     * @brief Create test vector data for VectorPointVisualization
     * @return Tuple of (x_coords, y_coords, row_indicators)
     */
    std::tuple<std::vector<float>, std::vector<float>, std::vector<int64_t>> createTestVectorData() {
        std::vector<float> x_coords = {10.0f, 15.0f, 20.0f, 30.0f, 35.0f, 50.0f, 55.0f, 60.0f, 65.0f};
        std::vector<float> y_coords = {20.0f, 25.0f, 30.0f, 40.0f, 45.0f, 60.0f, 65.0f, 70.0f, 75.0f};
        std::vector<int64_t> row_indicators = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        
        return {x_coords, y_coords, row_indicators};
    }
    
    /**
     * @brief Get the OpenGL context
     * @return Pointer to the OpenGL context
     */
    QOpenGLContext* getContext() const { return m_context.get(); }
    
    /**
     * @brief Get the offscreen surface
     * @return Pointer to the offscreen surface
     */
    QOffscreenSurface* getSurface() const { return m_surface.get(); }
    
    /**
     * @brief Get the Qt application
     * @return Pointer to the Qt application
     */
    QApplication* getApplication() const { return m_app.get(); }
    
    /**
     * @brief Process Qt events
     */
    void processEvents() {
        if (m_app) {
            m_app->processEvents();
        }
    }
    
    /**
     * @brief Wait for a specified number of milliseconds
     * @param ms Milliseconds to wait
     */
    void wait(int ms) {
        QTimer::singleShot(ms, [this]() {
            processEvents();
        });
        processEvents();
    }

private:
    std::unique_ptr<QApplication> m_app;
    std::unique_ptr<QOffscreenSurface> m_surface;
    std::unique_ptr<QOpenGLContext> m_context;
};

/**
 * @brief Test fixture specifically for point visualization testing
 * 
 * This fixture extends QtOpenGLTestFixture and provides additional utilities
 * for testing point visualization components.
 */
class PointVisualizationTestFixture : public QtOpenGLTestFixture {
protected:
    PointVisualizationTestFixture() : QtOpenGLTestFixture() {
        // Additional setup specific to point visualization testing
    }
    
    /**
     * @brief Create a test GroupManager
     * @return Pointer to GroupManager
     */
    GroupManager* createTestGroupManager() {
        // This would create a GroupManager with test groups
        // Implementation depends on GroupManager interface
        return nullptr; // Placeholder
    }
    
    /**
     * @brief Verify that a BoundingBox contains expected bounds
     * @param bounds The BoundingBox to verify
     * @param expected_min_x Expected minimum X coordinate
     * @param expected_min_y Expected minimum Y coordinate
     * @param expected_max_x Expected maximum X coordinate
     * @param expected_max_y Expected maximum Y coordinate
     */
    void verifyBoundingBox(const BoundingBox& bounds, 
                          float expected_min_x, float expected_min_y,
                          float expected_max_x, float expected_max_y) {
        REQUIRE(bounds.min_x == Catch::Approx(expected_min_x));
        REQUIRE(bounds.min_y == Catch::Approx(expected_min_y));
        REQUIRE(bounds.max_x == Catch::Approx(expected_max_x));
        REQUIRE(bounds.max_y == Catch::Approx(expected_max_y));
    }
    
    /**
     * @brief Verify that vertex data contains expected number of points
     * @param vertex_data The vertex data vector
     * @param expected_point_count Expected number of points (3 floats per point)
     */
    void verifyVertexDataSize(const std::vector<float>& vertex_data, size_t expected_point_count) {
        REQUIRE(vertex_data.size() == expected_point_count * 3); // x, y, group_id per point
    }
};

/**
 * @brief Test fixture for Qt widget testing without OpenGL requirements
 * 
 * This fixture provides a simplified testing environment for Qt widgets that don't
 * require OpenGL rendering, using offscreen rendering for headless testing.
 */
class QtWidgetTestFixture {
protected:
    QtWidgetTestFixture() {
        // Create a minimal Qt application for testing
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            // qputenv("QT_QPA_PLATFORM", "xcb");
        }

        // Disable Qt Wayland warnings/logging
        qputenv("QT_LOGGING_RULES", "qt.qpa.wayland*=false");

        // Create a minimal Qt application for testing
        static int argc = 1;
        static char* argv[] = {const_cast<char*>("test")};
        if (!QApplication::instance()) {
            m_app = std::make_unique<QApplication>(argc, argv);
        }
        
        QString platformName = QGuiApplication::platformName();
        std::cout << "Platform name: " << platformName.toStdString() << std::endl;
        if (platformName.contains("wayland", Qt::CaseInsensitive)) {
            // FAIL("Still using Wayland platform despite override");
        }
    }
    
    ~QtWidgetTestFixture() {
        // Clean up any remaining widgets
        QApplication::processEvents();
        QApplication::closeAllWindows();

        if (m_app) {
            m_app->processEvents();
            m_app->quit();
        }
    }
    
    /**
     * @brief Get the Qt application
     * @return Pointer to the Qt application
     */
    QApplication* getApplication() const { return m_app.get(); }
    
    /**
     * @brief Process Qt events
     */
    void processEvents() {
        if (m_app) {
            m_app->processEvents();
        }
    }
    
    /**
     * @brief Wait for a specified number of milliseconds
     * @param ms Milliseconds to wait
     */
    void wait(int ms) {
        QTimer::singleShot(ms, [this]() {
            processEvents();
        });
        processEvents();
    }

private:
    std::unique_ptr<QApplication> m_app;
};

#endif // QT_TEST_FIXTURES_HPP 