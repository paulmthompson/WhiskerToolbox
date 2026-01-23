/**
 * @file MinimalDockingTest.cpp
 * @brief Absolutely minimal ADS test - no config flags at all
 * 
 * This test uses ADS with zero configuration to verify basic inner docking works.
 */

#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>

#include "DockManager.h"
#include "DockWidget.h"
#include "DockAreaWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    qDebug() << "===========================================";
    qDebug() << "MINIMAL ADS TEST - No config flags";
    qDebug() << "===========================================";
    
    // NO config flags - use pure defaults
    qDebug() << "Using ADS defaults (no setConfigFlags call)";
    qDebug() << "Default config flags:" << static_cast<int>(ads::CDockManager::configFlags());
    
    QMainWindow window;
    window.setWindowTitle("Minimal ADS Test");
    window.resize(1000, 700);
    
    // Create dock manager with main window as parent
    // This makes it the central widget automatically
    auto* dockManager = new ads::CDockManager(&window);
    
    // Create 3 simple dock widgets
    for (int i = 1; i <= 3; ++i) {
        auto* dock = new ads::CDockWidget(QString("Widget %1").arg(i));
        
        auto* content = new QWidget();
        auto* layout = new QVBoxLayout(content);
        auto* label = new QLabel(QString("Drag this widget's tab!\n\nWidget %1").arg(i));
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 18px; padding: 20px; background: white;");
        layout->addWidget(label);
        
        dock->setWidget(content);
        
        // Add all to center - they become tabs
        dockManager->addDockWidget(ads::CenterDockWidgetArea, dock);
        
        qDebug() << "Added" << dock->objectName();
    }
    
    qDebug() << "";
    qDebug() << "TEST: Drag Widget 2 or Widget 3 tab over Widget 1";
    qDebug() << "You should see inner docking overlay arrows";
    qDebug() << "";
    
    window.show();
    return app.exec();
}
