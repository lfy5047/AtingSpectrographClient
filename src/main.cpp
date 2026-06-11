#include <QApplication>
#include <QIcon>
#include "AppVersion.h"
#include "Ui/MainWindow.h"
#include "Ui/ThemeManager.h"
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
    app.setWindowIcon(QIcon(":/icons/app_icon.png"));

    QFont font(QString::fromUtf8("微软雅黑"), 10);
    app.setFont(font);

    ThemeManager::applySavedTheme(app);

    MainWindow w;
    w.show();

    return app.exec();
}
