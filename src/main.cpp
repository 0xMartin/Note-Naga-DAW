#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QString>

#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Set application icon
    app.setWindowIcon(QIcon(":/icons/logo.svg"));

    // load application theme
    QFile theme(":/note_naga_theme.qss");
    if (theme.open(QFile::ReadOnly))
    {
        qDebug() << "Theme loaded successfully";
    }
    else
    {
        qDebug() << "Theme loading failed";
    }
    app.setStyleSheet(theme.readAll());

    // application metadata
    app.setOrganizationName("0xM4R71N");
    app.setOrganizationDomain("0xM4R71N");
    app.setApplicationName("Note Naga");

    MainWindow mw;
    mw.show();
    return app.exec();
}