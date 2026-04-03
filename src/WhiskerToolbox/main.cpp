#include "Main_Window/mainwindow.hpp"

#include "color_scheme.hpp"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QObject>
#include <QPalette>
#include <QSurfaceFormat>
#include <qstylefactory.h>

#include <spdlog/spdlog.h>

#include <algorithm>

// Ensure HDF5Explorer registration is linked when HDF5 is enabled
#ifdef ENABLE_HDF5
#include "HDF5Explorer_Widget/HDF5ExplorerRegistration.hpp"
#endif

#include "FileExplorer_Widgets/NpyExplorer_Widget/NpyExplorerRegistration.hpp"

#ifndef NDEBUG
#include "LayoutTesting/LayoutSanityChecker.hpp"
#endif

// Event filter to disable mouse wheel scrolling on combo boxes
class ComboBoxWheelFilter : public QObject {
public:
    explicit ComboBoxWheelFilter(QObject * parent = nullptr)
        : QObject(parent) {}

protected:
    bool eventFilter(QObject * obj, QEvent * event) override {
        if (event->type() == QEvent::Wheel) {
            // Check if the object is a QComboBox or its descendant
            if (qobject_cast<QComboBox *>(obj)) {
                auto * combo = qobject_cast<QComboBox *>(obj);
                // Only block wheel events if the combo box doesn't have focus
                // This allows intentional scrolling when the user has clicked into it
                if (!combo->hasFocus()) {
                    return true;// Filter out the event
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};


int main(int argc, char * argv[]) {
    // Parse --debug flag before QApplication to enable verbose logging.
    // Usage: WhiskerToolbox --debug
    bool const debug_logging = std::any_of(argv, argv + argc, [](char const * arg) {
        return std::string_view(arg) == "--debug";
    });
    if (debug_logging) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled");
    }

    // https://githubuser0xffff.github.io/Qt-Advanced-Docking-System/doc/user-guide.html#opengl--ads
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Explicitly trigger HDF5Explorer registration (static lib needs explicit reference)
#ifdef ENABLE_HDF5
    HDF5ExplorerRegistration::registerHDF5Explorer();
#endif

    // Explicitly trigger NpyExplorer registration (static lib needs explicit reference)
    NpyExplorerRegistration::registerNpyExplorer();

#ifdef Q_OS_LINUX
    // Force X11 backend on Linux for proper Qt Advanced Docking System support.
    // Wayland has issues with translucent overlay widgets and frameless window repositioning
    // used for dock drop indicators. This must be set BEFORE creating QApplication.
    //
    // See these issues for Wayland fix progress:
    //   https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System/issues/714
    //   https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System/pull/789
    //
    // Users can override by setting WHISKER_USE_WAYLAND=1 environment variable.
    if (qEnvironmentVariableIsEmpty("WHISKER_USE_WAYLAND")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

    // Set global OpenGL format for the application
    QSurfaceFormat format;
    format.setOption(QSurfaceFormat::DebugContext);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 3);// Use 4.3 for SpatialOverlayOpenGLWidget compatibility
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);

    // Install global event filter to disable accidental mouse wheel scrolling on combo boxes
    auto * wheelFilter = new ComboBoxWheelFilter(&a);
    a.installEventFilter(wheelFilter);

#ifndef NDEBUG
    // Layout sanity checker — logs warnings when widgets are squished below minimumSizeHint
    LayoutSanityChecker layoutChecker(&a);
#endif

    //a.setStyle("Fusion");

#ifdef __linux__
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif

    //QFile file(":/my_stylesheet.qss");
    //file.open(QFile::ReadOnly);
    //QString styleSheet { QLatin1String(file.readAll()) };
    //a.setStyleSheet(styleSheet);

    QApplication::setStyle(QStyleFactory::create("fusion"));

    QPalette const palette = create_palette();

    QApplication::setPalette(palette);

    MainWindow w;

    w.show();

    return QApplication::exec();
}
