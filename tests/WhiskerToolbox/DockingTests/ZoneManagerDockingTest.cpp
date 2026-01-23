/**
 * @file ZoneManagerDockingTest.cpp
 * @brief Test application for docking behavior with ZoneManager
 * 
 * This test uses ZoneManager to set up zones similar to the main application,
 * then tests whether inner docking still works.
 * 
 * Compare behavior with BasicDockingTest to identify if ZoneManager
 * causes docking issues.
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

#include "ZoneManager.hpp"

class ZoneTestMainWindow : public QMainWindow {
    Q_OBJECT
public:
    ZoneTestMainWindow(QWidget* parent = nullptr) : QMainWindow(parent), _widget_counter(0) {
        setWindowTitle("ZoneManager Docking Test");
        resize(1200, 800);
        
        // Remove central widget if any (same as main app)
        if (centralWidget()) {
            auto* central = takeCentralWidget();
            delete central;
        }
        
        qDebug() << "Setting up ADS with same config as main application...";
        
        // Use EXACTLY the same flags as the main application
        ads::CDockManager::setConfigFlags(
            ads::CDockManager::DefaultOpaqueConfig |
            ads::CDockManager::OpaqueSplitterResize |
            ads::CDockManager::DragPreviewIsDynamic
        );
        
        qDebug() << "Current config flags:" << ads::CDockManager::configFlags();
        
        // Create the dock manager
        _dock_manager = new ads::CDockManager(this);
        
        // Create ZoneManager (same as main app)
        _zone_manager = std::make_unique<ZoneManager>(_dock_manager, this);
        
        // Initialize zones (creates placeholder widgets)
        _zone_manager->initializeZones();
        
        // Set zone ratios (same as main app)
        _zone_manager->setZoneWidthRatios(0.20f, 0.58f, 0.22f);
        _zone_manager->setBottomHeightRatio(0.14f);
        
        // Create menu
        setupMenu();
        
        // Create initial widgets in zones
        createLeftWidget("Data Manager");
        createCenterWidget("Media Widget 1");
        createRightWidget("Properties");
        createBottomWidget("Timeline");
        
        // Reapply splitter sizes after layout
        _zone_manager->reapplySplitterSizes(200);
        
        qDebug() << "";
        qDebug() << "ZoneManagerDockingTest initialized";
        qDebug() << "";
        qDebug() << "TEST INSTRUCTIONS:";
        qDebug() << "1. Create a new Media Widget from File menu";
        qDebug() << "2. This will appear as a tab in the Center zone";
        qDebug() << "3. Try dragging the new 'Media Widget 2' tab";
        qDebug() << "4. Attempt to dock it to the LEFT/RIGHT/TOP/BOTTOM of 'Media Widget 1'";
        qDebug() << "5. Do you see inner docking zones, or only outer (window edge) zones?";
        qDebug() << "";
    }

private slots:
    void createNewCenterWidget() {
        ++_widget_counter;
        createCenterWidget(QString("Media Widget %1").arg(_widget_counter + 1));
    }

private:
    void setupMenu() {
        auto* fileMenu = menuBar()->addMenu("File");
        
        auto* newAction = new QAction("New Media Widget (Center Zone)", this);
        newAction->setShortcut(QKeySequence::New);
        connect(newAction, &QAction::triggered, this, &ZoneTestMainWindow::createNewCenterWidget);
        fileMenu->addAction(newAction);
        
        auto* quitAction = new QAction("Quit", this);
        quitAction->setShortcut(QKeySequence::Quit);
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
        fileMenu->addAction(quitAction);
    }
    
    ads::CDockWidget* createDockWidget(const QString& title, Qt::GlobalColor color) {
        auto* dock = new ads::CDockWidget(title);
        
        // Create a simple colored widget with some content
        auto* content = new QWidget();
        auto* layout = new QVBoxLayout(content);
        
        auto* label = new QLabel(title);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QString("background-color: %1; font-size: 18px; padding: 10px;")
            .arg(QColor(color).name()));
        
        auto* textEdit = new QTextEdit();
        textEdit->setPlaceholderText(QString("Content for %1").arg(title));
        
        layout->addWidget(label);
        layout->addWidget(textEdit);
        
        dock->setWidget(content);
        
        dock->setFeature(ads::CDockWidget::DockWidgetClosable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetMovable, true);
        dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
        
        return dock;
    }
    
    void createLeftWidget(const QString& title) {
        auto* dock = createDockWidget(title, Qt::lightGray);
        _zone_manager->addToZone(dock, Zone::Left, true);
        qDebug() << "Created LEFT zone widget:" << title;
    }
    
    void createCenterWidget(const QString& title) {
        auto* dock = createDockWidget(title, Qt::white);
        _zone_manager->addToZone(dock, Zone::Center, true);
        qDebug() << "Created CENTER zone widget:" << title;
    }
    
    void createRightWidget(const QString& title) {
        auto* dock = createDockWidget(title, Qt::yellow);
        _zone_manager->addToZone(dock, Zone::Right, true);
        qDebug() << "Created RIGHT zone widget:" << title;
    }
    
    void createBottomWidget(const QString& title) {
        auto* dock = createDockWidget(title, Qt::cyan);
        _zone_manager->addToZone(dock, Zone::Bottom, true);
        qDebug() << "Created BOTTOM zone widget:" << title;
    }
    
    ads::CDockManager* _dock_manager;
    std::unique_ptr<ZoneManager> _zone_manager;
    int _widget_counter;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    qDebug() << "========================================";
    qDebug() << "ZoneManager Docking Test";
    qDebug() << "========================================";
    qDebug() << "";
    
    ZoneTestMainWindow window;
    window.show();
    
    return app.exec();
}

#include "ZoneManagerDockingTest.moc"
