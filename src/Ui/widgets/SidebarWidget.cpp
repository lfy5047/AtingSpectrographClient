#include "SidebarWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>
#include <QPropertyAnimation>
#include <QStyle>

// nav item data: label, icon type
static const struct { const char* label; int iconType; } kNavItems[] = {
    {"仪表盘", 0},
    {"设备连接", 1},
    {"相机设置", 2},
    {"转镜控制", 3},
    {"红外热像", 4},
    {"数据采集", 5},
    {"流通道", 6},
    {"系统日志", 7},
};

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("sidebar");
    setFixedWidth(kExpandedW);
    setupUi();
}

void SidebarWidget::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Logo area ──
    logoArea_ = new QWidget(this);
    logoArea_->setObjectName("sidebarLogo");
    logoArea_->setFixedHeight(58);
    auto* logoLayout = new QHBoxLayout(logoArea_);
    logoLayout->setContentsMargins(12, 10, 12, 10);
    logoLayout->setSpacing(10);

    logoIcon_ = new QLabel(logoArea_);
    logoIcon_->setObjectName("sidebarLogoIcon");
    logoIcon_->setFixedSize(32, 32);
    logoIcon_->setAlignment(Qt::AlignCenter);
    {
        QPixmap pm(32, 32);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QLinearGradient g(0, 0, 32, 32);
        g.setColorAt(0, QColor(0x4C, 0x8E, 0xF7));
        g.setColorAt(1, QColor(0x7C, 0x6F, 0xF7));
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, 32, 32, 6, 6);
        p.setPen(Qt::white);
        QFont f("Microsoft YaHei UI", 14, QFont::Bold);
        p.setFont(f);
        p.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, "A");
        p.end();
        logoIcon_->setPixmap(pm);
    }

    logoText_ = new QLabel("AtingSpectrograph", logoArea_);
    logoText_->setObjectName("sidebarLogoText");

    logoLayout->addWidget(logoIcon_);
    logoLayout->addWidget(logoText_, 1);
    root->addWidget(logoArea_);

    // ── Nav area ──
    navArea_ = new QWidget(this);
    auto* navLayout = new QVBoxLayout(navArea_);
    navLayout->setContentsMargins(4, 4, 4, 4);
    navLayout->setSpacing(0);

    QColor iconColor(0x7D, 0x85, 0x90);

    for (int i = 0; i < 8; ++i) {
        auto* btn = new QPushButton(navArea_);
        btn->setObjectName("navItem");
        btn->setText(QString::fromUtf8(kNavItems[i].label));
        btn->setProperty("fullText", QString::fromUtf8(kNavItems[i].label));
        btn->setIcon(QIcon(makeIcon(kNavItems[i].iconType, iconColor)));
        btn->setIconSize(QSize(20, 20));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(36);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            setActivePanel(i);
            emit panelSelected(i);
        });
        navBtns_.append(btn);
        navLayout->addWidget(btn);
    }
    navLayout->addStretch();
    root->addWidget(navArea_, 1);

    // ── Footer area ──
    footerArea_ = new QWidget(this);
    footerArea_->setObjectName("sidebarFooter");
    auto* footerLayout = new QVBoxLayout(footerArea_);
    footerLayout->setContentsMargins(4, 6, 4, 6);
    footerLayout->setSpacing(2);

    // connection indicator
    auto* connRow = new QHBoxLayout();
    connRow->setContentsMargins(8, 4, 8, 4);
    connDot_ = new QLabel(footerArea_);
    connDot_->setObjectName("connDot");
    connDot_->setFixedSize(10, 10);
    connText_ = new QLabel(QString::fromUtf8("未连接"), footerArea_);
    connText_->setObjectName("sidebarConnText");
    connRow->addWidget(connDot_);
    connRow->addWidget(connText_, 1);
    footerLayout->addLayout(connRow);

    // collapse button
    collapseBtn_ = new QPushButton(QString::fromUtf8("折叠菜单"), footerArea_);
    collapseBtn_->setObjectName("sidebarCollapseBtn");
    collapseBtn_->setCursor(Qt::PointingHandCursor);
    collapseBtn_->setFixedHeight(32);
    connect(collapseBtn_, &QPushButton::clicked, this, [this]() {
        setCollapsed(!collapsed_);
    });
    footerLayout->addWidget(collapseBtn_);

    root->addWidget(footerArea_);

    setConnected(false);
    setActivePanel(0);
}

static void drawIcon(QPainter& p, int type, const QColor& c, int sz)
{
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    float s = sz / 24.0f;
    p.scale(s, s);

    switch (type) {
    case 0: // Dashboard — 2x2 grid of rounded rects
        for (int r = 0; r < 2; ++r)
            for (int c2 = 0; c2 < 2; ++c2)
                p.drawRoundedRect(3 + c2 * 9, 3 + r * 9, 7, 7, 1.5, 1.5);
        break;
    case 1: // Connection — link chain
        p.drawEllipse(QPointF(8, 12), 3.5, 3.5);
        p.drawLine(10, 10, 14, 14);
        p.drawEllipse(QPointF(16, 12), 3.5, 3.5);
        break;
    case 2: // Camera
        p.drawRoundedRect(3, 7, 18, 12, 2, 2);
        p.drawEllipse(QPointF(12, 13), 4, 4);
        p.drawRect(9, 4, 6, 4);
        break;
    case 3: // Mirror — circle with line to center
        p.drawEllipse(QPointF(12, 12), 9, 9);
        p.drawLine(12, 12, 12, 5);
        p.drawLine(12, 12, 16, 13);
        break;
    case 4: // IR — thermometer
        p.drawEllipse(QPointF(12, 19.5f), 3.5f, 3.5f);
    {
        QPainterPath path;
        path.addRoundedRect(10, 4, 4, 12, 2, 2);
        p.drawPath(path);
    }
        break;
    case 5: // Collect — 3 vertical bars
        p.drawLine(7, 20, 7, 6);
        p.drawLine(12, 20, 12, 4);
        p.drawLine(17, 20, 17, 10);
        break;
    case 6: // Stream — polyline wave
    {
        QPainterPath path;
        path.moveTo(2, 16);
        path.lineTo(6, 16);
        path.lineTo(9, 6);
        path.lineTo(15, 18);
        path.lineTo(18, 12);
        path.lineTo(22, 12);
        p.drawPath(path);
    }
        break;
    case 7: // Logs — document with lines
        p.drawRoundedRect(3, 3, 14, 18, 2, 2);
        p.drawLine(6, 9, 14, 9);
        p.drawLine(6, 13, 14, 13);
        p.drawLine(6, 17, 10, 17);
        {
            QPainterPath corner;
            corner.moveTo(17, 3);
            corner.lineTo(17, 8);
            corner.lineTo(12, 8);
            p.drawPath(corner);
        }
        break;
    }
}

QPixmap SidebarWidget::makeIcon(int type, const QColor& color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawIcon(p, type, color, size);
    p.end();
    return pm;
}

void SidebarWidget::setConnected(bool connected)
{
    connected_ = connected;
    connDot_->setStyleSheet(connected
        ? "background-color: #26A641; border-radius: 5px; min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px;"
        : "background-color: #545D68; border-radius: 5px; min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px;");
    connText_->setText(connected
        ? QString::fromUtf8("已连接")
        : QString::fromUtf8("未连接"));
}

void SidebarWidget::setActivePanel(int index)
{
    if (index < 0 || index >= navBtns_.size()) return;
    activeIndex_ = index;
    QColor active(0x4C, 0x8E, 0xF7);
    QColor normal(0x7D, 0x85, 0x90);
    for (int i = 0; i < navBtns_.size(); ++i) {
        navBtns_[i]->setProperty("active", i == index);
        navBtns_[i]->style()->polish(navBtns_[i]);
        navBtns_[i]->setIcon(QIcon(makeIcon(kNavItems[i].iconType, i == index ? active : normal)));
    }
}

void SidebarWidget::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    int target = collapsed ? kCollapsedW : kExpandedW;

    if (!anim_) {
        anim_ = new QPropertyAnimation(this, "sidebarWidth", this);
        anim_->setDuration(220);
        anim_->setEasingCurve(QEasingCurve::InOutQuad);
    }
    anim_->setStartValue(width());
    anim_->setEndValue(target);
    anim_->start();

    logoText_->setVisible(!collapsed);
    connText_->setVisible(!collapsed);
    collapseBtn_->setText(collapsed ? "" : QString::fromUtf8("折叠菜单"));

    for (auto* btn : navBtns_) {
        if (collapsed) {
            btn->setText("");
            btn->setToolTip(btn->property("fullText").toString());
        } else {
            btn->setText(btn->property("fullText").toString());
            btn->setToolTip("");
        }
    }

    emit collapseToggled(collapsed);
}
