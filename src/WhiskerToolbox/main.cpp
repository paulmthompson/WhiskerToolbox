#include "Main_Window/mainwindow.hpp"

#include <QApplication>
#include <qstylefactory.h>
#include <QFile>

#include "jkqtplotter/jkqtplotter.h"
#include "jkqtplotter/graphs/jkqtplines.h"

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    a.setStyle("Fusion");

#ifdef __linux__
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif

    MainWindow w;

    QFile file(":/my_stylesheet.qss");
    file.open(QFile::ReadOnly);
    QString styleSheet { QLatin1String(file.readAll()) };
    a.setStyleSheet(styleSheet);

    if (a.palette().window().color().value() > a.palette().windowText().color().value()){
        
    }

    w.show();

    // JKQT Test
    // JKQTPlotter plot;
    // JKQTPDatastore* ds=plot.getDatastore();
    // QVector<double> X, Y;
    // const int Ndata=100;
    // for (int i=0; i<Ndata; i++) {
    //     const double x=double(i)/double(Ndata)*8.0*M_PI;
    //     X<<x;
    //     Y<<sin(x);
    // }
    // size_t columnX=ds->addCopiedColumn(X, "x");
    // size_t columnY=ds->addCopiedColumn(Y, "y");
    // JKQTPXYLineGraph* graph1=new JKQTPXYLineGraph(&plot);
    // graph1->setXColumn(columnX);
    // graph1->setYColumn(columnY);
    // graph1->setTitle(QObject::tr("sine graph"));
    // plot.addGraph(graph1);
    // plot.zoomToFit();
    // plot.show();
    // plot.resize(600,400);

    return a.exec();
}
