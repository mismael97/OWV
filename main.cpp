#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VCD Wave Viewer");
    app.setApplicationVersion("1.0");

    MainWindow w;
    w.show();
    return app.exec();
}
