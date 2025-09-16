#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Analysis_Dashboard.hpp"
#include "DataManager.hpp"
#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "TimeScrollBar.hpp"
#include "ShaderManager/ShaderManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "Toolbox/ToolboxPanel.hpp"
#include "Properties/PropertiesPanel.hpp"
#include "Analysis_Dashboard/PlotOrganizers/PlotDockWidgetContent.hpp"
#include "Analysis_Dashboard/PlotOrganizers/DockingPlotOrganizer.hpp"
#include "Analysis_Dashboard/PlotContainer.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotPropertiesWidget.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "CoreGeometry/ImageSize.hpp"
#include "Analysis_Dashboard/Plots/AbstractPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayPlotWidget.hpp"
#include "Analysis_Dashboard/Widgets/SpatialOverlayPlotWidget/SpatialOverlayOpenGLWidget.hpp"
#include "Analysis_Dashboard/Widgets/Common/ViewState.hpp"

#include "DockManager.h"
#include <QApplication>
#include <QTimer>
#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>


#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QVector>
#include <QImage>

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
    
    // Create dashboard with null data manager should throw an exception
    ads::CDockManager dock_manager;
    REQUIRE_THROWS_AS(
        Analysis_Dashboard(nullptr, time_scrollbar, &dock_manager),
        std::runtime_error
    );
    
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

TEST_CASE_METHOD(QtTestFixture, "Analysis Dashboard - Properties panel switches per plot selection and data isolation", "[AnalysisDashboard][Integration]") {
    auto is_uniform_background = [](QImage const & img) -> bool {
        if (img.isNull()) return true;
        // Sample a grid of pixels and check if close to 0.95 gray (~242)
        auto is_bg = [](QRgb c) {
            int r = qRed(c), g = qGreen(c), b = qBlue(c);
            return std::abs(r - 242) <= 6 && std::abs(g - 242) <= 6 && std::abs(b - 242) <= 6;
        };
        int w = img.width(), h = img.height();
        if (w <= 2 || h <= 2) return true;
        int samples = 0, bg_count = 0;
        for (int y = 1; y < h; y += std::max(1, h / 8)) {
            for (int x = 1; x < w; x += std::max(1, w / 8)) {
                QRgb c = img.pixel(x, y);
                bg_count += is_bg(c) ? 1 : 0;
                samples++;
            }
        }
        // Consider uniform if >90% samples are background
        return samples > 0 && bg_count * 10 >= samples * 9;
    };
    // Create a data manager and time scrollbar
    auto data_manager = std::make_shared<DataManager>();
    REQUIRE(data_manager != nullptr);

    auto time_scrollbar = new TimeScrollBar();
    REQUIRE(time_scrollbar != nullptr);

    // Seed DataManager with PointData (test_points)
    auto point_data = std::make_shared<PointData>();
    // Three frames of simple points
    point_data->overwritePointsAtTime(TimeFrameIndex(1), std::vector<Point2D<float>>{{10.0f, 10.0f}, {20.0f, 20.0f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(2), std::vector<Point2D<float>>{{30.0f, 30.0f}});
    point_data->overwritePointsAtTime(TimeFrameIndex(3), std::vector<Point2D<float>>{{40.0f, 10.0f}, {50.0f, 15.0f}});
    point_data->setImageSize(ImageSize(800, 600));
    data_manager->setData<PointData>("test_points", point_data, TimeKey("time"));

    // Global dock manager
    ads::CDockManager dock_manager;

    // Create dashboard
    Analysis_Dashboard dashboard(data_manager, time_scrollbar, &dock_manager);
    dashboard.show();
    QApplication::processEvents();

    // Find the toolbox panel hosted inside the dashboard
    auto toolbox_list = dashboard.findChildren<ToolboxPanel*>();
    REQUIRE_FALSE(toolbox_list.empty());
    ToolboxPanel* toolbox = toolbox_list.front();
    REQUIRE(toolbox != nullptr);

    // Find the internal list and add button to add plots via UI path
    QListWidget* plot_list = toolbox->findChild<QListWidget*>("plot_list");
    REQUIRE(plot_list != nullptr);
    QPushButton* add_button = toolbox->findChild<QPushButton*>("add_button");
    REQUIRE(add_button != nullptr);

    // Helper: select "Spatial Overlay Plot" item by display text and click Add
    auto add_spatial_overlay_plot = [&]() {
        QList<QListWidgetItem*> matches = plot_list->findItems("Spatial Overlay Plot", Qt::MatchExactly);
        REQUIRE_FALSE(matches.isEmpty());
        plot_list->setCurrentItem(matches.first());
        REQUIRE(plot_list->currentItem() == matches.first());
        QTest::mouseClick(add_button, Qt::LeftButton);
        QApplication::processEvents();
    };

    // Add two spatial overlay plots
    add_spatial_overlay_plot();
    add_spatial_overlay_plot();

    // The docking organizer should have created two plot content widgets (children of dock manager)
    QTest::qWait(50);
    // Collect all PlotDockWidgetContent instances from the entire app
    QVector<PlotDockWidgetContent*> spatial_contents;
    for (QWidget* w : QApplication::allWidgets()) {
        if (auto* c = qobject_cast<PlotDockWidgetContent*>(w)) {
            spatial_contents.push_back(c);
        }
    }
    REQUIRE(spatial_contents.size() >= 2);

    // Find the properties panel and its stacked widget current widget indirectly
    auto props_list = dashboard.findChildren<PropertiesPanel*>();
    REQUIRE_FALSE(props_list.empty());
    PropertiesPanel* properties_panel = props_list.front();
    REQUIRE(properties_panel != nullptr);

    // Helper to get current properties widget pointer by peeking the stacked widget child
    auto get_current_properties_widget = [&]() -> QWidget* {
        QStackedWidget* stacked = properties_panel->findChild<QStackedWidget*>();
        REQUIRE(stacked != nullptr);
        return stacked->currentWidget();
    };

    // Click first plot content and capture current properties widget
    QTest::mouseClick(spatial_contents[0], Qt::LeftButton);
    QApplication::processEvents();
    QWidget* first_props = get_current_properties_widget();
    REQUIRE(first_props != nullptr);
    REQUIRE(qobject_cast<SpatialOverlayPlotPropertiesWidget*>(first_props) != nullptr);

    // Enable 'test_points' dataset in the first plot's properties via Feature_Table_Widget
    {
        auto* props_widget = qobject_cast<SpatialOverlayPlotPropertiesWidget*>(first_props);
        REQUIRE(props_widget != nullptr);
        // Ensure table populated
        props_widget->updateAvailableDataSources();
        QApplication::processEvents();

        // Find the feature table and locate the row for 'test_points'
        auto* feature_table = first_props->findChild<QTableWidget*>("available_features_table");
        REQUIRE(feature_table != nullptr);

        int feature_col = -1;
        int enabled_col = -1;
        for (int col = 0; col < feature_table->columnCount(); ++col) {
            auto* header = feature_table->horizontalHeaderItem(col);
            if (!header) { continue; }
            QString text = header->text();
            if (text.compare("Feature", Qt::CaseInsensitive) == 0) feature_col = col;
            if (text.compare("Enabled", Qt::CaseInsensitive) == 0) enabled_col = col;
        }
        REQUIRE(feature_col >= 0);
        REQUIRE(enabled_col >= 0);

        int target_row = -1;
        for (int row = 0; row < feature_table->rowCount(); ++row) {
            auto* item = feature_table->item(row, feature_col);
            if (item && item->text() == QStringLiteral("test_points")) {
                target_row = row;
                break;
            }
        }
        REQUIRE(target_row >= 0);

        // Toggle the checkbox in Enabled column
        QWidget* cell_widget = feature_table->cellWidget(target_row, enabled_col);
        REQUIRE(cell_widget != nullptr);
        QCheckBox* checkbox = cell_widget->findChild<QCheckBox*>();
        REQUIRE(checkbox != nullptr);

        if (!checkbox->isChecked()) {
            QTest::mouseClick(checkbox, Qt::LeftButton);
            QApplication::processEvents();
        }
    }

    // Click second plot content and capture current properties widget
    QTest::mouseClick(spatial_contents[1], Qt::LeftButton);
    QApplication::processEvents();
    QWidget* second_props = get_current_properties_widget();
    REQUIRE(second_props != nullptr);
    REQUIRE(qobject_cast<SpatialOverlayPlotPropertiesWidget*>(second_props) != nullptr);

    // Ensure different properties widget instances are shown for each plot
    REQUIRE(first_props != second_props);

    // Click back to first and verify it switches back
    QTest::mouseClick(spatial_contents[0], Qt::LeftButton);
    QApplication::processEvents();
    QWidget* first_props_again = get_current_properties_widget();
    REQUIRE(first_props_again == first_props);

    // Verify only plot 1 has the point dataset attached
    auto organizers = dashboard.findChildren<DockingPlotOrganizer*>();
    REQUIRE_FALSE(organizers.empty());
    DockingPlotOrganizer* org = organizers.front();
    REQUIRE(org != nullptr);

    // Extract plot ids from content object names
    auto extract_id = [](PlotDockWidgetContent* c) {
        QString name = c->objectName();
        QString prefix = QStringLiteral("PlotDockWidgetContent_");
        int idx = name.indexOf(prefix);
        if (idx >= 0) return name.mid(prefix.length());
        return name;
    };
    QString id1 = extract_id(spatial_contents[0]);
    QString id2 = extract_id(spatial_contents[1]);

    PlotContainer* container1 = org->getPlot(id1);
    PlotContainer* container2 = org->getPlot(id2);
    REQUIRE(container1 != nullptr);
    REQUIRE(container2 != nullptr);

    auto* plot1 = qobject_cast<SpatialOverlayPlotWidget*>(container1->getPlotWidget());
    auto* plot2 = qobject_cast<SpatialOverlayPlotWidget*>(container2->getPlotWidget());
    REQUIRE(plot1 != nullptr);
    REQUIRE(plot2 != nullptr);
    

    auto keys1 = plot1->getPointDataKeys();
    auto keys2 = plot2->getPointDataKeys();
    REQUIRE(keys1.contains("test_points"));
    REQUIRE_FALSE(keys2.contains("test_points"));

    // Helper: count total enabled datasets per plot (points + masks + lines)
    auto enabled_count = [](SpatialOverlayPlotWidget* p) -> int {
        int n = 0;
        n += p->getPointDataKeys().size();
        n += p->getMaskDataKeys().size();
        n += p->getLineDataKeys().size();
        return n;
    };
    REQUIRE(enabled_count(plot1) == 1);
    REQUIRE(enabled_count(plot2) == 0);
    // Diagnostic spies attached; emissions may be zero so far depending on when data was applied

    // Verify via ViewState: plot 1 has data bounds, plot 2 does not (initially)
    bool framebuffer_checks_supported = false;
    if (plot1->getOpenGLWidget()) {
        auto const & vs1 = plot1->getOpenGLWidget()->getViewState();
        REQUIRE(vs1.data_bounds.width() > 0.0f);
        REQUIRE(vs1.data_bounds.height() > 0.0f);
        // Try to capture framebuffer; if non-uniform, we will use stricter visual checks later
        QApplication::processEvents();
        QTest::qWait(50);
        QImage img1_initial = plot1->getOpenGLWidget()->grabFramebuffer();
        framebuffer_checks_supported = !is_uniform_background(img1_initial);
    }
    if (plot2->getOpenGLWidget()) {
        auto const & vs2 = plot2->getOpenGLWidget()->getViewState();
        REQUIRE(vs2.data_bounds.width() == 0.0f);
        REQUIRE(vs2.data_bounds.height() == 0.0f);
    }

    // Now enable the same dataset in plot 2
    // Record plot 1 view state to ensure it remains unchanged after enabling on plot 2
    ViewState vs1_before = plot1->getOpenGLWidget()->getViewState();
    QTest::mouseClick(spatial_contents[1], Qt::LeftButton);
    QApplication::processEvents();
    second_props = get_current_properties_widget();
    {
        auto* props_widget2 = qobject_cast<SpatialOverlayPlotPropertiesWidget*>(second_props);
        REQUIRE(props_widget2 != nullptr);
        props_widget2->updateAvailableDataSources();
        QApplication::processEvents();
        auto* feature_table2 = second_props->findChild<QTableWidget*>("available_features_table");
        REQUIRE(feature_table2 != nullptr);
        int feature_col2 = -1, enabled_col2 = -1;
        for (int col = 0; col < feature_table2->columnCount(); ++col) {
            auto* header = feature_table2->horizontalHeaderItem(col);
            if (!header) continue;
            QString text = header->text();
            if (text.compare("Feature", Qt::CaseInsensitive) == 0) feature_col2 = col;
            if (text.compare("Enabled", Qt::CaseInsensitive) == 0) enabled_col2 = col;
        }
        REQUIRE(feature_col2 >= 0);
        REQUIRE(enabled_col2 >= 0);
        int target_row2 = -1;
        for (int row = 0; row < feature_table2->rowCount(); ++row) {
            auto* item = feature_table2->item(row, feature_col2);
            if (item && item->text() == QStringLiteral("test_points")) { target_row2 = row; break; }
        }
        REQUIRE(target_row2 >= 0);
        QWidget* cell_widget2 = feature_table2->cellWidget(target_row2, enabled_col2);
        REQUIRE(cell_widget2 != nullptr);
        QCheckBox* checkbox2 = cell_widget2->findChild<QCheckBox*>();
        REQUIRE(checkbox2 != nullptr);
        if (!checkbox2->isChecked()) {
            QTest::mouseClick(checkbox2, Qt::LeftButton);
            QApplication::processEvents();
        }
    }

    // Assert plot 1's view state is unchanged
    {
        ViewState vs1_after = plot1->getOpenGLWidget()->getViewState();
        REQUIRE(vs1_after.zoom_level_x == Catch::Approx(vs1_before.zoom_level_x));
        REQUIRE(vs1_after.zoom_level_y == Catch::Approx(vs1_before.zoom_level_y));
        REQUIRE(vs1_after.pan_offset_x == Catch::Approx(vs1_before.pan_offset_x));
        REQUIRE(vs1_after.pan_offset_y == Catch::Approx(vs1_before.pan_offset_y));
        REQUIRE(vs1_after.data_bounds.min_x == Catch::Approx(vs1_before.data_bounds.min_x));
        REQUIRE(vs1_after.data_bounds.min_y == Catch::Approx(vs1_before.data_bounds.min_y));
        REQUIRE(vs1_after.data_bounds.max_x == Catch::Approx(vs1_before.data_bounds.max_x));
        REQUIRE(vs1_after.data_bounds.max_y == Catch::Approx(vs1_before.data_bounds.max_y));
        REQUIRE(vs1_after.data_bounds_valid == vs1_before.data_bounds_valid);
    }

    // After enabling on plot 2: verify plot 2 has data bounds (renders)
    if (plot2->getOpenGLWidget()) {
        auto const & vs2b = plot2->getOpenGLWidget()->getViewState();
        REQUIRE(vs2b.data_bounds.width() > 0.0f);
        REQUIRE(vs2b.data_bounds.height() > 0.0f);
    }
    REQUIRE(enabled_count(plot2) == 1);
    REQUIRE(enabled_count(plot1) == 1);


    // Switch back to plot 1 properties and confirm UI still shows enabled
    QTest::mouseClick(spatial_contents[0], Qt::LeftButton);
    QApplication::processEvents();
    first_props = get_current_properties_widget();
    {
        auto* feature_table = first_props->findChild<QTableWidget*>("available_features_table");
        REQUIRE(feature_table != nullptr);
        int feature_col = -1, enabled_col = -1;
        for (int col = 0; col < feature_table->columnCount(); ++col) {
            auto* header = feature_table->horizontalHeaderItem(col);
            if (!header) continue;
            QString text = header->text();
            if (text.compare("Feature", Qt::CaseInsensitive) == 0) feature_col = col;
            if (text.compare("Enabled", Qt::CaseInsensitive) == 0) enabled_col = col;
        }
        REQUIRE(feature_col >= 0);
        REQUIRE(enabled_col >= 0);
        int target_row = -1;
        for (int row = 0; row < feature_table->rowCount(); ++row) {
            auto* item = feature_table->item(row, feature_col);
            if (item && item->text() == QStringLiteral("test_points")) { target_row = row; break; }
        }
        REQUIRE(target_row >= 0);
        QWidget* cell_widget = feature_table->cellWidget(target_row, enabled_col);
        REQUIRE(cell_widget != nullptr);
        QCheckBox* checkbox = cell_widget->findChild<QCheckBox*>();
        REQUIRE(checkbox != nullptr);
        REQUIRE(checkbox->isChecked());
    }

    // Check plot 1 via ViewState: Keep asserting it still has data bounds (expected correct behavior)
    if (plot1->getOpenGLWidget()) {
        auto const & vs1b = plot1->getOpenGLWidget()->getViewState();
        REQUIRE(vs1b.data_bounds.width() > 0.0f);
        REQUIRE(vs1b.data_bounds.height() > 0.0f);
        // If framebuffer looked valid earlier, also assert visually non-uniform now
        if (framebuffer_checks_supported) {
            QApplication::processEvents();
            QTest::qWait(50);
            QImage img1_after = plot1->getOpenGLWidget()->grabFramebuffer();
            REQUIRE_FALSE(is_uniform_background(img1_after));
        }
    }

    // Now reproduce toggle sequence described by user:
    // 1) Click to plot 2 and disable: points disappear on plot 2 and (in manual behavior) come back on plot 1
    {
        QTest::mouseClick(spatial_contents[1], Qt::LeftButton);
        QApplication::processEvents();
        QWidget* props2 = get_current_properties_widget();
        auto* feature_table2 = props2->findChild<QTableWidget*>("available_features_table");
        REQUIRE(feature_table2 != nullptr);
        int feature_col2 = -1, enabled_col2 = -1;
        for (int col = 0; col < feature_table2->columnCount(); ++col) {
            auto* header = feature_table2->horizontalHeaderItem(col);
            if (!header) continue;
            QString text = header->text();
            if (text.compare("Feature", Qt::CaseInsensitive) == 0) feature_col2 = col;
            if (text.compare("Enabled", Qt::CaseInsensitive) == 0) enabled_col2 = col;
        }
        REQUIRE(feature_col2 >= 0);
        REQUIRE(enabled_col2 >= 0);
        int row2 = -1;
        for (int row = 0; row < feature_table2->rowCount(); ++row) {
            auto* item = feature_table2->item(row, feature_col2);
            if (item && item->text() == QStringLiteral("test_points")) { row2 = row; break; }
        }
        REQUIRE(row2 >= 0);
        QWidget* cell2 = feature_table2->cellWidget(row2, enabled_col2);
        REQUIRE(cell2 != nullptr);
        QCheckBox* cb2 = cell2->findChild<QCheckBox*>();
        REQUIRE(cb2 != nullptr);
        if (cb2->isChecked()) {
            QTest::mouseClick(cb2, Qt::LeftButton);
            QApplication::processEvents();
        }

        // Plot 2 should now have zero data bounds and zero enabled datasets
        if (plot2->getOpenGLWidget()) {
            auto const & vs2c = plot2->getOpenGLWidget()->getViewState();
            REQUIRE(vs2c.data_bounds.width() == 0.0f);
            REQUIRE(vs2c.data_bounds.height() == 0.0f);
        }
        REQUIRE(enabled_count(plot2) == 0);


        // Plot 1 should show data (reappears) — assert positive bounds
        if (plot1->getOpenGLWidget()) {
            auto const & vs1c = plot1->getOpenGLWidget()->getViewState();
            REQUIRE(vs1c.data_bounds.width() > 0.0f);
            REQUIRE(vs1c.data_bounds.height() > 0.0f);
        }
    }

    // 2) Click disable on plot 1 — expected behavior: it should clear and show zero bounds
    // This reproduces the bug if bounds remain positive and/or keys remain set.
    {
        QTest::mouseClick(spatial_contents[0], Qt::LeftButton);
        QApplication::processEvents();
        QWidget* props1 = get_current_properties_widget();
        auto* feature_table1 = props1->findChild<QTableWidget*>("available_features_table");
        REQUIRE(feature_table1 != nullptr);
        int feature_col1 = -1, enabled_col1 = -1;
        for (int col = 0; col < feature_table1->columnCount(); ++col) {
            auto* header = feature_table1->horizontalHeaderItem(col);
            if (!header) continue;
            QString text = header->text();
            if (text.compare("Feature", Qt::CaseInsensitive) == 0) feature_col1 = col;
            if (text.compare("Enabled", Qt::CaseInsensitive) == 0) enabled_col1 = col;
        }
        REQUIRE(feature_col1 >= 0);
        REQUIRE(enabled_col1 >= 0);
        int row1 = -1;
        for (int row = 0; row < feature_table1->rowCount(); ++row) {
            auto* item = feature_table1->item(row, feature_col1);
            if (item && item->text() == QStringLiteral("test_points")) { row1 = row; break; }
        }
        REQUIRE(row1 >= 0);
        QWidget* cell1 = feature_table1->cellWidget(row1, enabled_col1);
        REQUIRE(cell1 != nullptr);
        QCheckBox* cb1 = cell1->findChild<QCheckBox*>();
        REQUIRE(cb1 != nullptr);
        if (cb1->isChecked()) {
            QTest::mouseClick(cb1, Qt::LeftButton);
            QApplication::processEvents();
        }

        // Expectation (correct behavior): plot 1 clears data and shows zero enabled datasets
        // Known bug (as described): it may still render and keys remain.
        // Keep the assertions as the expected behavior; if they fail, we reproduced the bug.
        if (plot1->getOpenGLWidget()) {
            auto const & vs1d = plot1->getOpenGLWidget()->getViewState();
            REQUIRE(vs1d.data_bounds.width() == 0.0f);
            REQUIRE(vs1d.data_bounds.height() == 0.0f);
        }
        REQUIRE(plot1->getPointDataKeys().isEmpty());
        REQUIRE(enabled_count(plot1) == 0);

    }

    // Optional: remove plot 2 and verify plot 1 state remains stable
    {
        REQUIRE(org->getPlotCount() >= 2);
        bool removed = org->removePlot(id2);
        REQUIRE(removed);
        REQUIRE(org->getPlotCount() >= 1);

        // Plot 1 still accessible and its view/bounds unchanged (already zero in the last step)
        if (plot1->getOpenGLWidget()) {
            auto const & vs1e = plot1->getOpenGLWidget()->getViewState();
            // Just assert it remains a valid state (data_bounds_valid set by view update)
            REQUIRE(vs1e.widget_width > 0);
            REQUIRE(vs1e.widget_height > 0);
        }
    }

    // Clean up
    delete time_scrollbar;
}
