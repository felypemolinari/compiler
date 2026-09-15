#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Compiler IDE");
    QApplication::setOrganizationName("Felype Molinari and Vinicius Andriani");

    // Fusion honours the custom palette used by the dark mode on every platform.
    QApplication::setStyle("Fusion");

    MainWindow window;
    window.show();

    return app.exec();
}
