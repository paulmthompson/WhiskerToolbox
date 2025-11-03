#include "Main_Window/mainwindow.hpp"

#include "color_scheme.hpp"

#include <QApplication>
#include <qstylefactory.h>
#include <QFile>
#include <QPalette>
#include <QSurfaceFormat>
#include <QComboBox>
#include <QEvent>
#include <QObject>

// Event filter to disable mouse wheel scrolling on combo boxes
class ComboBoxWheelFilter : public QObject
{
public:
    explicit ComboBoxWheelFilter(QObject* parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel) {
            // Check if the object is a QComboBox or its descendant
            if (qobject_cast<QComboBox*>(obj)) {
                QComboBox* combo = qobject_cast<QComboBox*>(obj);
                // Only block wheel events if the combo box doesn't have focus
                // This allows intentional scrolling when the user has clicked into it
                if (!combo->hasFocus()) {
                    return true; // Filter out the event
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};



int main(int argc, char *argv[])
{
    // Set global OpenGL format for the application
    QSurfaceFormat format;
    format.setOption(QSurfaceFormat::DebugContext);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 3); // Use 4.3 for SpatialOverlayOpenGLWidget compatibility
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);

    // Install global event filter to disable accidental mouse wheel scrolling on combo boxes
    ComboBoxWheelFilter* wheelFilter = new ComboBoxWheelFilter(&a);
    a.installEventFilter(wheelFilter);

    //a.setStyle("Fusion");

#ifdef __linux__
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif

    //QFile file(":/my_stylesheet.qss");
    //file.open(QFile::ReadOnly);
    //QString styleSheet { QLatin1String(file.readAll()) };
    //a.setStyleSheet(styleSheet);

    QApplication::setStyle(QStyleFactory::create("fusion"));

    QPalette palette = create_palette();

    QApplication::setPalette(palette);

    MainWindow w;

    w.show();

    return a.exec();
}
