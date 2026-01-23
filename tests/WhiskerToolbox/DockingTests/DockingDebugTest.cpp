/**
 * @file DockingDebugTest.cpp
 * @brief Test application with detailed debug output for docking behavior
 * 
 * This test helps diagnose why inner docking might not work by:
 * 1. Logging overlay activity
 * 2. Checking widget hierarchy
 * 3. Verifying dock container state
 */

#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QTimer>

#include "DockManager.h"
#include "DockWidget.h"
#include "DockAreaWidget.h"
#include "DockContainerWidget.h"
#include "DockSplitter.h"

/**
 * Helper function to dump the dock widget hierarchy
 */
void dumpDockHierarchy(ads::CDockManager* manager, const QString& context = "Current State") {
    qDebug() << "";
    qDebug() << "======== DOCK HIERARCHY:" << context << "========";
    
    // List all dock widgets
    auto dockWidgets = manager->dockWidgetsMap();
    qDebug() << "Total dock widgets:" << dockWidgets.size();
    for (auto it = dockWidgets.begin(); it != dockWidgets.end(); ++it) {
        auto* dock = it.value();
        auto* area = dock->dockAreaWidget();
        qDebug() << "  -" << dock->objectName() 
                 << "| closed:" << dock->isClosed()
                 << "| visible:" << dock->isVisible()
                 << "| area:" << (area ? QString::number(reinterpret_cast<quintptr>(area), 16) : "none");
    }
    
    // List all dock areas  
    qDebug() << "";
    qDebug() << "Dock Areas:";
    
    // CDockManager IS a CDockContainerWidget, so use it directly
    for (int i = 0; i < manager->dockAreaCount(); ++i) {
        auto* area = manager->dockArea(i);
        qDebug() << "  Area" << i << ":" 
                 << QString::number(reinterpret_cast<quintptr>(area), 16)
                 << "| dockWidgets:" << area->dockWidgetsCount()
                 << "| visible:" << area->isVisible();
    }
    
    qDebug() << "========================================";
    qDebug() << "";
}

class DebugMainWindow : public QMainWindow {
    Q_OBJECT
public:
    DebugMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), _widget_counter(0) {
        setWindowTitle("Docking Debug Test");
        resize(1200, 800);
        
        qDebug() << "=======================================================";
        qDebug() << "DOCKING DEBUG TEST";
        qDebug() << "=======================================================";
        qDebug() << "";
        
        // ===================================================================
        // TEST DIFFERENT CONFIGURATION FLAG COMBINATIONS
        // ===================================================================
        
        qDebug() << "Testing configuration flags...";
        
        // Configuration 1: Default Opaque (what the main app uses)
        ads::CDockManager::ConfigFlags flags = 
            ads::CDockManager::DefaultOpaqueConfig |
            ads::CDockManager::OpaqueSplitterResize |
            ads::CDockManager::DragPreviewIsDynamic;
            
        qDebug() << "Config flags value:" << static_cast<int>(flags);
        qDebug() << "";
        
        // Log individual flag states
        qDebug() << "Individual flags enabled:";
        qDebug() << "  - OpaqueSplitterResize:" << bool(flags & ads::CDockManager::OpaqueSplitterResize);
        qDebug() << "  - DragPreviewIsDynamic:" << bool(flags & ads::CDockManager::DragPreviewIsDynamic);
        qDebug() << "  - DockAreaHasCloseButton:" << bool(flags & ads::CDockManager::DockAreaHasCloseButton);
        qDebug() << "  - DockAreaHasUndockButton:" << bool(flags & ads::CDockManager::DockAreaHasUndockButton);
        qDebug() << "";
        
        ads::CDockManager::setConfigFlags(flags);
        
        // Create the dock manager
        _dock_manager = new ads::CDockManager(this);
        
        // Setup signal/slot monitoring for dock events
        connect(_dock_manager, &ads::CDockManager::dockAreaCreated, this, [](ads::CDockAreaWidget* area) {
            qDebug() << "SIGNAL: dockAreaCreated:" << QString::number(reinterpret_cast<quintptr>(area), 16);
        });
        
        connect(_dock_manager, &ads::CDockManager::dockWidgetAdded, this, [](ads::CDockWidget* dock) {
            qDebug() << "SIGNAL: dockWidgetAdded:" << dock->objectName();
        });
        
        connect(_dock_manager, &ads::CDockManager::dockWidgetRemoved, this, [](ads::CDockWidget* dock) {
            qDebug() << "SIGNAL: dockWidgetRemoved:" << dock->objectName();
        });
        
        // Create menu
        setupMenu();
        
        qDebug() << "";
        qDebug() << "Creating initial layout...";
        
        // Create a simple layout with 3 widgets
        // This mirrors the main app's zone-based approach
        
        // First widget - "left panel"
        auto* left_dock = createDockWidget("Left Panel", Qt::lightGray);
        auto* left_area = _dock_manager->addDockWidget(ads::LeftDockWidgetArea, left_dock);
        qDebug() << "Added 'Left Panel' -> area:" << QString::number(reinterpret_cast<quintptr>(left_area), 16);
        
        // Second widget - "center" (first one in center becomes main)
        auto* center_dock1 = createDockWidget("Center 1", Qt::white);
        auto* center_area = _dock_manager->addDockWidget(ads::CenterDockWidgetArea, center_dock1);
        qDebug() << "Added 'Center 1' -> area:" << QString::number(reinterpret_cast<quintptr>(center_area), 16);
        
        // Third widget - tabbed with center (same area)
        auto* center_dock2 = createDockWidget("Center 2", Qt::cyan);
        _dock_manager->addDockWidget(ads::CenterDockWidgetArea, center_dock2, center_area);
        qDebug() << "Added 'Center 2' -> tabbed with Center 1";
        
        // Fourth widget - right panel
        auto* right_dock = createDockWidget("Right Panel", Qt::yellow);
        auto* right_area = _dock_manager->addDockWidget(ads::RightDockWidgetArea, right_dock, center_area);
        qDebug() << "Added 'Right Panel' -> area:" << QString::number(reinterpret_cast<quintptr>(right_area), 16);
        
        qDebug() << "";
        dumpDockHierarchy(_dock_manager, "After initial setup");
        
        qDebug() << "";
        qDebug() << "=======================================================";
        qDebug() << "TEST INSTRUCTIONS:";
        qDebug() << "";
        qDebug() << "1. You should see 3 dock areas: Left, Center (with 2 tabs), Right";
        qDebug() << "";
        qDebug() << "2. Try dragging 'Center 2' tab:";
        qDebug() << "   - Drag toward the CENTER of 'Center 1' content area";
        qDebug() << "   - You should see inner drop indicators (arrows pointing inward)";
        qDebug() << "";
        qDebug() << "3. If you only see OUTER drop indicators (on window edges),";
        qDebug() << "   the inner docking is broken.";
        qDebug() << "";
        qDebug() << "4. Try dragging to dock 'Center 2' to the LEFT of 'Center 1'";
        qDebug() << "   This should split the center area vertically.";
        qDebug() << "";
        qDebug() << "5. Use 'View -> Dump Hierarchy' to see current state.";
        qDebug() << "=======================================================";
        qDebug() << "";
    }

private slots:
    void createNewWidget() {
        auto* dock = createDockWidget(QString("New Widget %1").arg(++_widget_counter), Qt::magenta);
        
        // Add to center area by finding it (CDockManager IS a CDockContainerWidget)
        if (_dock_manager->dockAreaCount() > 0) {
            // Find a center-ish area (not first, not last)
            int target_idx = _dock_manager->dockAreaCount() / 2;
            auto* target_area = _dock_manager->dockArea(target_idx);
            _dock_manager->addDockWidget(ads::CenterDockWidgetArea, dock, target_area);
        } else {
            _dock_manager->addDockWidget(ads::CenterDockWidgetArea, dock);
        }
        
        dumpDockHierarchy(_dock_manager, "After adding new widget");
    }
    
    void dumpHierarchy() {
        dumpDockHierarchy(_dock_manager, "User requested dump");
    }

private:
    void setupMenu() {
        auto* fileMenu = menuBar()->addMenu("File");
        
        auto* newAction = new QAction("New Widget", this);
        newAction->setShortcut(QKeySequence::New);
        connect(newAction, &QAction::triggered, this, &DebugMainWindow::createNewWidget);
        fileMenu->addAction(newAction);
        
        fileMenu->addSeparator();
        
        auto* quitAction = new QAction("Quit", this);
        quitAction->setShortcut(QKeySequence::Quit);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
        fileMenu->addAction(quitAction);
        
        auto* viewMenu = menuBar()->addMenu("View");
        
        auto* dumpAction = new QAction("Dump Hierarchy", this);
        dumpAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
        connect(dumpAction, &QAction::triggered, this, &DebugMainWindow::dumpHierarchy);
        viewMenu->addAction(dumpAction);
    }
    
    ads::CDockWidget* createDockWidget(const QString& title, Qt::GlobalColor color) {
        auto* dock = new ads::CDockWidget(title);
        
        auto* content = new QWidget();
        auto* layout = new QVBoxLayout(content);
        
        auto* label = new QLabel(title);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString("background-color: %1; font-size: 20px; padding: 15px;")
            .arg(QColor(color).name()));
        
        auto* textEdit = new QTextEdit();
        textEdit->setPlaceholderText(QString("Drag this dock widget's TAB to test docking.\n\n"
            "When dragging over another dock area, you should see:\n"
            "- Center drop zone (makes tabs)\n"  
            "- Left/Right/Top/Bottom arrows (splits the area)\n\n"
            "If you only see drop zones on window edges, inner docking is broken."));
        
        layout->addWidget(label);
        layout->addWidget(textEdit);
        
        dock->setWidget(content);
        
        // Enable all features
        dock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
        
        return dock;
    }
    
    ads::CDockManager* _dock_manager;
    int _widget_counter;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    DebugMainWindow window;
    window.show();
    
    return app.exec();
}

#include "DockingDebugTest.moc"
