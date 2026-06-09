#include <QApplication>
#include <QFile>
#include "AppVersion.h"
#include "Ui/MainWindow.h"
#include "Client/stream/StreamFrame.h"
#include "plog/Initializers/RollingFileInitializer.h"
#include "plog/Appenders/ColorConsoleAppender.h"

int main(int argc, char* argv[])
{

    plog::init(plog::debug, "log/log.txt", 1024 * 1024, 10);
    plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::get()->addAppender(&consoleAppender);

    QApplication app(argc, argv);
    qRegisterMetaType<StreamFrame>("StreamFrame");
    app.setOrganizationName(AppVersion::organizationName());
    app.setApplicationName(AppVersion::applicationName());
    app.setApplicationVersion(AppVersion::version());

    QFont font(QString::fromUtf8("微软雅黑"), 10);
    app.setFont(font);

    QFile qss(":/style/industrial.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow w;
    w.show();

    return app.exec();
}
