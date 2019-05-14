#include "mainwindow.h"

#include <QApplication>
#include <QMessageBox>

using namespace std;


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    try {
        MainWindow m;
        m.show();
        return app.exec();
    }
    catch(const exception& ex)
    { QMessageBox(QMessageBox::Critical, "Error", ex.what(), QMessageBox::Close).exec(); }
    catch(...)
    { QMessageBox(QMessageBox::Critical, "Error", "Unhandled exception!", QMessageBox::Close).exec(); }
    return -1;
}
