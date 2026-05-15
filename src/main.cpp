#include <QApplication>
#include <QFile>
#include "Ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("AtingSpectrograph");
    app.setApplicationName("AtingSpectrographClient");

    QFont font(QString::fromUtf8("微软雅黑"), 10);
    app.setFont(font);

    QFile qss(":/style/industrial.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow w;
    w.show();

    return app.exec();
}
