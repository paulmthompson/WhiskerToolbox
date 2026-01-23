/**
 * @file BasicDockingTest.cpp
 * @brief Minimal test application for Qt-Advanced-Docking-System
 * 
 * This test verifies that basic ADS docking features work:
 * - Creating multiple dock widgets
 * - Dragging and dropping to split views (not just outer docking)
 * - Inner docking (docking relative to individual widgets, not just window edges)
 * 
 * Run this test to verify ADS works correctly without ZoneManager.
 * If docking works here but not in the main app, the issue is with ZoneManager.
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

#include "DockManager.h"
#include "DockWidget.h"
#include "DockAreaWidget.h"

class TestMainWindow : public QMainWindow {
    Q_OBJECT
public:
    TestMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), _widget_counter(0) {
        setWindowTitle("Basic ADS Docking Test");
        resize(1200, 800);
        
        // ===========================================================================
        // IMPORTANT: Configuration flags control docking behavior
        // ===========================================================================
        // DefaultOpaqueConfig is a composite flag that includes:
        // - OpaqueSplitterResize
        // - OpaqueUndocking  
        // - OpaqueDocking
        // 
        // Key flags for inner docking:
        // - DockAreaHasUndockButton: Enable undock button on dock areas
        // - DockAreaDynamicTabsMenuButtonVisibility: Show tab menu dynamically
        // - AllowOuterDocking: Allow docking to main window edges
        //
        // The flag that controls inner docking (docking relative to other widgets):
        // Inner docking is ENABLED BY DEFAULT unless you disable it
        // ===========================================================================
        
        qDebug() << "Setting up ADS with default config flags...";
        
        // Use the same flags as the main application for comparison
        ads::CDockManager::setConfigFlags(
            ads::CDockManager::DefaultOpaqueConfig |
            ads::CDockManager::OpaqueSplitterResize |
            ads::CDockManager::DragPreviewIsDynamic
        );
        
        qDebug() << "Current config flags:" << ads::CDockManager::configFlags();
        
        // Create the dock manager
        _dock_manager = new ads::CDockManager(this);
        
        // Create initial dock widgets
        createDockWidget("Editor 1", Qt::blue);
        createDockWidget("Editor 2", Qt::green);
        createDockWidget("Properties", Qt::yellow);
        
        // Create menu
        setupMenu();
        
        qDebug() << "BasicDockingTest initialized with 3 dock widgets";
        qDebug() << "";
        qDebug() << "TEST INSTRUCTIONS:";
        qDebug() << "1. Try dragging 'Editor 2' tab onto 'Editor 1' area";
        qDebug() << "2. You should see overlay icons to dock LEFT/RIGHT/TOP/BOTTOM relative to Editor 1";
        qDebug() << "3. These are the 'inner docking' zones - not just window edges";
        qDebug() << "4. If you only see outer (window edge) docking, something is wrong";
        qDebug() << "";
    }

private slots:
    void createNewWidget() {
        createDockWidget(QString("Widget %1").arg(++_widget_counter), Qt::cyan);
    }

private:
    void setupMenu() {
        auto* fileMenu = menuBar()->addMenu("File");
        
        auto* newAction = new QAction("New Widget", this);
        newAction->setShortcut(QKeySequence::New);
        connect(newAction, &QAction::triggered, this, &TestMainWindow::createNewWidget);
        fileMenu->addAction(newAction);
        
        auto* quitAction = new QAction("Quit", this);
        quitAction->setShortcut(QKeySequence::Quit);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
        fileMenu->addAction(quitAction);
    }
    
    void createDockWidget(const QString& title, Qt::GlobalColor color) {
        auto* dock = new ads::CDockWidget(title);
        
        // Create a simple colored widget with some content
        auto* content = new QWidget();
        auto* layout = new QVBoxLayout(content);
        
        auto* label = new QLabel(title);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString("background-color: %1; font-size: 24px; padding: 20px;")
            .arg(QColor(color).name()));
        
        auto* textEdit = new QTextEdit();
        textEdit->setPlaceholderText(QString("Content area for %1\n\nTry dragging this dock widget's tab to another dock area.").arg(title));
        
        layout->addWidget(label);
        layout->addWidget(textEdit);
        
        dock->setWidget(content);
        
        // Make dock widget closable and floatable (standard features)
        dock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
        
        // Add to dock manager - first widget becomes center, others tab
        _dock_manager->addDockWidget(ads::CenterDockWidgetArea, dock);
        
        qDebug() << "Created dock widget:" << title;
    }
    
    ads::CDockManager* _dock_manager;
    int _widget_counter;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    qDebug() << "========================================";
    qDebug() << "Basic ADS Docking Test";
    qDebug() << "========================================";
    qDebug() << "";
    
    TestMainWindow window;
    window.show();
    
    return app.exec();
}

#include "BasicDockingTest.moc"
