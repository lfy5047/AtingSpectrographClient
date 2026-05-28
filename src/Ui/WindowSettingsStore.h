#pragma once

class QMainWindow;
class QSplitter;
class SidebarWidget;

class WindowSettingsStore {
public:
    static int restore(QMainWindow* window, QSplitter* splitter, SidebarWidget* sidebar);
    static void save(QMainWindow* window, QSplitter* splitter, SidebarWidget* sidebar, int fallbackPanel);
};
