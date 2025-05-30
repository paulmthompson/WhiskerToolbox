#include "Main_Window/mainwindow.hpp"

#include "color_scheme.hpp"

#include <QApplication>
#include <qstylefactory.h>
#include <QFile>
#include <QPalette>
#include <QSurfaceFormat>



int main(int argc, char *argv[])
{

    //QSurfaceFormat format;
    //format.setOption(QSurfaceFormat::DebugContext);
    //format.setProfile(QSurfaceFormat::CoreProfile);
    //format.setVersion(4, 1);
    //QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);

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
