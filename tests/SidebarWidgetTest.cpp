#include <QtTest/QtTest>

#include "Ui/widgets/SidebarWidget.h"

#include <QPushButton>

namespace {

QImage buttonIconImage(const QPushButton* button)
{
    return button->icon().pixmap(button->iconSize()).toImage();
}

} // namespace

class SidebarWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void tempControlIconDiffersFromIrIcon()
    {
        SidebarWidget sidebar;
        const QList<QPushButton*> buttons = sidebar.findChildren<QPushButton*>("navItem");

        QVERIFY2(buttons.size() >= 6, "Sidebar should expose IR and temperature control nav buttons");
        const QImage irIcon = buttonIconImage(buttons.at(4));
        const QImage tempControlIcon = buttonIconImage(buttons.at(5));

        QVERIFY2(!irIcon.isNull(), "IR nav icon should render");
        QVERIFY2(!tempControlIcon.isNull(), "Temperature control nav icon should render");
        QVERIFY2(irIcon != tempControlIcon, "Temperature control nav icon should differ from IR nav icon");
    }
};

QTEST_MAIN(SidebarWidgetTest)
#include "SidebarWidgetTest.moc"
