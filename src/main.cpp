#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    QApplication app(argc, argv);
    app.setApplicationName("MatBin");
    app.setOrganizationName("Yuvanesh");
    app.setApplicationVersion("1.3.0");

    app.setStyle("Fusion");

    MainWindow window;
    window.show();

    return app.exec();
}