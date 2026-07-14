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
    void exposesRefactoredNavigation()
    {
        SidebarWidget sidebar;
        const QList<QPushButton*> buttons = sidebar.findChildren<QPushButton*>("navItem");
        const QStringList expected = {
            QString::fromUtf8("仪表盘"),
            QString::fromUtf8("数据采集"),
            QString::fromUtf8("探测器设置"),
            QString::fromUtf8("校正"),
            QString::fromUtf8("温控控制"),
            QString::fromUtf8("录制回放"),
            QString::fromUtf8("光谱分析"),
            QString::fromUtf8("光谱段测试"),
            QString::fromUtf8("Binning 测试"),
            QString::fromUtf8("ROI 测试"),
            QString::fromUtf8("高级设置"),
            QString::fromUtf8("系统日志"),
        };

        QCOMPARE(buttons.size(), expected.size());
        for (int i = 0; i < expected.size(); ++i) {
            QCOMPARE(buttons.at(i)->property("fullText").toString(), expected.at(i));
        }

        const QList<int> expectedPanelIndices = {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 7, 11};
        QSignalSpy panelSpy(&sidebar, &SidebarWidget::panelSelected);
        for (int i = 0; i < buttons.size(); ++i) {
            buttons.at(i)->click();
            QCOMPARE(panelSpy.count(), 1);
            QCOMPARE(panelSpy.takeFirst().at(0).toInt(), expectedPanelIndices.at(i));
        }
    }

    void tempControlIconDiffersFromIrIcon()
    {
        SidebarWidget sidebar;
        const QList<QPushButton*> buttons = sidebar.findChildren<QPushButton*>("navItem");

        QVERIFY2(buttons.size() >= 5, "Sidebar should expose detector and temperature control nav buttons");
        const QImage irIcon = buttonIconImage(buttons.at(2));
        const QImage tempControlIcon = buttonIconImage(buttons.at(4));

        QVERIFY2(!irIcon.isNull(), "IR nav icon should render");
        QVERIFY2(!tempControlIcon.isNull(), "Temperature control nav icon should render");
        QVERIFY2(irIcon != tempControlIcon, "Temperature control nav icon should differ from IR nav icon");
    }
};

QTEST_MAIN(SidebarWidgetTest)
#include "SidebarWidgetTest.moc"
