#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QList>
#include <QPropertyAnimation>

class SidebarWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth)
public:
    explicit SidebarWidget(QWidget* parent = nullptr);

    void setConnected(bool connected);
    void setActivePanel(int index);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return collapsed_; }
    int sidebarWidth() const { return width(); }
    void setSidebarWidth(int w) { setFixedWidth(w); }

signals:
    void panelSelected(int index);
    void collapseToggled(bool collapsed);

private:
    void setupUi();
    static QPixmap makeIcon(int type, const QColor& color, int size = 20);

    QWidget*   logoArea_    = nullptr;
    QLabel*    logoIcon_    = nullptr;
    QLabel*    logoText_    = nullptr;
    QWidget*   navArea_     = nullptr;
    QList<QPushButton*> navBtns_;
    QWidget*   footerArea_  = nullptr;
    QLabel*    connDot_     = nullptr;
    QLabel*    connText_    = nullptr;
    QPushButton* collapseBtn_ = nullptr;

    QPropertyAnimation* anim_ = nullptr;

    bool collapsed_   = false;
    int  activeIndex_ = 0;
    bool connected_   = false;

    static const int kExpandedW  = 210;
    static const int kCollapsedW = 56;
};
