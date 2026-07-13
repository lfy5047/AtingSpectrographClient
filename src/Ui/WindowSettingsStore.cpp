#include "WindowSettingsStore.h"

#include "MainWindowPanelRegistry.h"
#include "widgets/SidebarWidget.h"

#include <QMainWindow>
#include <QSettings>
#include <QSplitter>

int WindowSettingsStore::restore(QMainWindow* window, QSplitter* splitter, SidebarWidget* sidebar)
{
    QSettings s;
    if (window) {
        window->restoreGeometry(s.value("window/geometry").toByteArray());
    }
    if (splitter) {
        splitter->restoreState(s.value("window/splitter").toByteArray());
    }

    int panel = s.value("window/panel", 0).toInt();
    const int panelVersion = s.value("window/panelVersion", 1).toInt();
    if (panelVersion < 2) {
        if (panel == 7) panel = 8;
        else if (panel == 8) panel = 9;
    }
    if (panelVersion < 4 && panel >= 5) {
        panel += 1;
    }

    if (panelVersion < 5) {
        switch (panel) {
        case 0:
            panel = MainWindowPanelRegistry::Dashboard;
            break;
        case 1:
        case 2:
            panel = MainWindowPanelRegistry::AdvancedSettings;
            break;
        case 3:
        case 6:
        case 7:
        case 8:
            panel = MainWindowPanelRegistry::DataAcquisition;
            break;
        case 4:
            panel = MainWindowPanelRegistry::DetectorSettings;
            break;
        case 5:
            panel = MainWindowPanelRegistry::TempControl;
            break;
        case 9:
            panel = MainWindowPanelRegistry::RecordPlayback;
            break;
        case 10:
            panel = MainWindowPanelRegistry::SpectrumAnalysis;
            break;
        case 11:
            panel = MainWindowPanelRegistry::Log;
            break;
        default:
            panel = MainWindowPanelRegistry::Dashboard;
            break;
        }
    }
    if (panelVersion < 6 && panel == 8) {
        panel = MainWindowPanelRegistry::Log;
    }
    if (panel < MainWindowPanelRegistry::Dashboard || panel > MainWindowPanelRegistry::Log) {
        panel = MainWindowPanelRegistry::Dashboard;
    }

    const bool sidebarCollapsed = s.value("window/sidebarCollapsed", false).toBool();
    if (sidebar) {
        sidebar->setCollapsed(sidebarCollapsed);
    }
    return panel;
}

void WindowSettingsStore::save(QMainWindow* window, QSplitter* splitter, SidebarWidget* sidebar, int fallbackPanel)
{
    QSettings s;
    if (window) {
        s.setValue("window/geometry", window->saveGeometry());
    }
    if (splitter) {
        s.setValue("window/splitter", splitter->saveState());
    }
    s.setValue("window/panel", sidebar ? sidebar->activePanel() : fallbackPanel);
    s.setValue("window/panelVersion", MainWindowPanelRegistry::PanelVersion);
    s.setValue("window/sidebarCollapsed", sidebar && sidebar->isCollapsed());
}
