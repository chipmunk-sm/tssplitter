#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(mpegts);

    QApplication app(argc, argv);

    QCoreApplication::setOrganizationDomain("");
    QCoreApplication::setOrganizationName("mpegts");
    QCoreApplication::setApplicationName("mpegts");
    QCoreApplication::setApplicationVersion("2.0.0");

    try
    {
        MainWindow m;
        m.show();
        return app.exec();
    }
    catch (const std::exception& ex)
    {
        QMessageBox::critical(nullptr, "Error", ex.what(), QMessageBox::Close);
    }
    catch (...)
    {
        QMessageBox::critical(nullptr, "Error", "Unhandled exception!", QMessageBox::Close);
    }
    return -1;
}
