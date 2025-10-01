#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("VCD Wave Viewer");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("VCDViewer");

    MainWindow window;
    window.show();

    return app.exec();
}
