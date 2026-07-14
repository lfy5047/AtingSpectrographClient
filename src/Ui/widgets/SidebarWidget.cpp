#include "SidebarWidget.h"

#include <QBrush>
#include <QHBoxLayout>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QPropertyAnimation>
#include <QStyle>
#include <QVBoxLayout>

static const struct { const char* label; int iconType; int panelIndex; } kNavItems[] = {
    {"仪表盘", 0, 0},
    {"数据采集", 5, 1},
    {"探测器设置", 4, 2},
    {"校正", 3, 3},
    {"温控控制", 11, 4},
    {"录制回放", 8, 5},
    {"光谱分析", 10, 6},
    {"光谱段测试", 12, 8},
    {"Binning 测试", 13, 9},
    {"ROI 测试", 14, 10},
    {"高级设置", 7, 7},
    {"系统日志", 6, 11},
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
        logoIcon_->setPixmap(pm);
    }

    logoText_ = new QLabel("AtingSpectrograph", logoArea_);
    logoText_->setObjectName("sidebarLogoText");
    logoLayout->addWidget(logoIcon_);
    logoLayout->addWidget(logoText_, 1);
    root->addWidget(logoArea_);

    navArea_ = new QWidget(this);
    navArea_->setObjectName("sidebarNav");
    auto* navLayout = new QVBoxLayout(navArea_);
    navLayout->setContentsMargins(4, 4, 4, 4);
    navLayout->setSpacing(0);

    QColor iconColor(0x7D, 0x85, 0x90);
    for (int i = 0; i < static_cast<int>(sizeof(kNavItems) / sizeof(kNavItems[0])); ++i) {
        auto* btn = new QPushButton(navArea_);
        btn->setObjectName("navItem");
        btn->setText(QString::fromUtf8(kNavItems[i].label));
        btn->setProperty("fullText", QString::fromUtf8(kNavItems[i].label));
        btn->setIcon(QIcon(makeIcon(kNavItems[i].iconType, iconColor)));
        btn->setIconSize(QSize(20, 20));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(36);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            const int panelIndex = kNavItems[i].panelIndex;
            setActivePanel(panelIndex);
            emit panelSelected(panelIndex);
        });
        navBtns_.append(btn);
        navLayout->addWidget(btn);
    }
    navLayout->addStretch();
    root->addWidget(navArea_, 1);

    footerArea_ = new QWidget(this);
    footerArea_->setObjectName("sidebarFooter");
    auto* footerLayout = new QVBoxLayout(footerArea_);
    footerLayout->setContentsMargins(4, 6, 4, 6);
    footerLayout->setSpacing(2);

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
    p.scale(sz / 24.0f, sz / 24.0f);

    switch (type) {
    case 0:
        for (int r = 0; r < 2; ++r)
            for (int c2 = 0; c2 < 2; ++c2)
                p.drawRoundedRect(3 + c2 * 9, 3 + r * 9, 7, 7, 1.5, 1.5);
        break;
    case 1:
        p.drawEllipse(QPointF(8, 12), 3.5, 3.5);
        p.drawLine(10, 10, 14, 14);
        p.drawEllipse(QPointF(16, 12), 3.5, 3.5);
        break;
    case 2:
        p.drawRoundedRect(3, 7, 18, 12, 2, 2);
        p.drawEllipse(QPointF(12, 13), 4, 4);
        p.drawRect(9, 4, 6, 4);
        break;
    case 3:
        p.drawEllipse(QPointF(12, 12), 9, 9);
        p.drawLine(12, 12, 12, 5);
        p.drawLine(12, 12, 16, 13);
        break;
    case 4:
        p.drawEllipse(QPointF(12, 19.5f), 3.5f, 3.5f);
        p.drawRoundedRect(10, 4, 4, 12, 2, 2);
        break;
    case 5:
        p.drawLine(7, 20, 7, 6);
        p.drawLine(12, 20, 12, 4);
        p.drawLine(17, 20, 17, 10);
        break;
    case 6: {
        QPainterPath path;
        path.moveTo(2, 16);
        path.lineTo(6, 16);
        path.lineTo(9, 6);
        path.lineTo(15, 18);
        path.lineTo(18, 12);
        path.lineTo(22, 12);
        p.drawPath(path);
        break;
    }
    case 7:
        p.drawRoundedRect(3, 3, 14, 18, 2, 2);
        p.drawLine(6, 9, 14, 9);
        p.drawLine(6, 13, 14, 13);
        p.drawLine(6, 17, 10, 17);
        break;
    case 8:
        p.drawEllipse(QPointF(8.0f, 12.0f), 2.5f, 2.5f);
        p.drawRoundedRect(12, 6, 9, 12, 2, 2);
        p.drawPolygon(QPolygonF() << QPointF(15, 9) << QPointF(15, 15) << QPointF(19, 12));
        break;
    case 9:
        p.drawRoundedRect(3, 4, 18, 16, 2, 2);
        p.drawLine(6, 16, 6, 8);
        p.drawLine(10, 16, 10, 6);
        p.drawLine(14, 16, 14, 10);
        p.drawLine(18, 16, 18, 12);
        break;
    case 10: {
        p.drawLine(3, 18, 21, 18);
        p.drawLine(4, 20, 4, 4);
        QPainterPath curve;
        curve.moveTo(5, 15);
        curve.cubicTo(8, 7, 11, 8, 13, 12);
        curve.cubicTo(15, 16, 18, 11, 21, 6);
        p.drawPath(curve);
        p.drawLine(5, 10, 21, 10);
        break;
    }
    case 11:
        p.drawRoundedRect(4, 5, 16, 14, 2, 2);
        p.drawLine(7, 10, 17, 10);
        p.drawLine(7, 15, 17, 15);
        p.setBrush(c);
        p.drawEllipse(QPointF(10, 10), 1.5, 1.5);
        p.drawEllipse(QPointF(15, 15), 1.5, 1.5);
        p.setBrush(Qt::NoBrush);
        break;
    case 12:
        p.drawRoundedRect(3, 4, 18, 16, 2, 2);
        p.drawLine(8, 6, 8, 18);
        p.drawLine(16, 6, 16, 18);
        p.drawLine(9.5, 12, 14.5, 12);
        p.drawLine(10.5, 10.5, 9, 12);
        p.drawLine(10.5, 13.5, 9, 12);
        p.drawLine(13.5, 10.5, 15, 12);
        p.drawLine(13.5, 13.5, 15, 12);
        break;
    case 13:
        p.drawRoundedRect(3, 3, 18, 18, 2, 2);
        p.drawRect(6, 6, 4, 4);
        p.drawRect(14, 6, 4, 4);
        p.drawRect(6, 14, 4, 4);
        p.drawRect(14, 14, 4, 4);
        break;
    case 14:
        p.drawRoundedRect(3, 3, 18, 18, 2, 2);
        p.drawLine(7, 7, 11, 7);
        p.drawLine(7, 7, 7, 11);
        p.drawLine(17, 7, 13, 7);
        p.drawLine(17, 7, 17, 11);
        p.drawLine(7, 17, 11, 17);
        p.drawLine(7, 17, 7, 13);
        p.drawLine(17, 17, 13, 17);
        p.drawLine(17, 17, 17, 13);
        break;
    }
}

QPixmap SidebarWidget::makeIcon(int type, const QColor& color, int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawIcon(p, type, color, size);
    return pm;
}

void SidebarWidget::setConnected(bool connected)
{
    connected_ = connected;
    connDot_->setStyleSheet(connected
        ? "background-color: #26A641; border-radius: 5px; min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px;"
        : "background-color: #545D68; border-radius: 5px; min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px;");
    connText_->setText(connected ? QString::fromUtf8("已连接") : QString::fromUtf8("未连接"));
}

void SidebarWidget::setActivePanel(int index)
{
    if (index < 0) return;
    activeIndex_ = index;
    QColor active(0x4C, 0x8E, 0xF7);
    QColor normal(0x7D, 0x85, 0x90);
    for (int i = 0; i < navBtns_.size(); ++i) {
        const bool isActive = (kNavItems[i].panelIndex == index);
        navBtns_[i]->setProperty("active", isActive);
        navBtns_[i]->style()->polish(navBtns_[i]);
        navBtns_[i]->setIcon(QIcon(makeIcon(kNavItems[i].iconType, isActive ? active : normal)));
    }
}

void SidebarWidget::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    const int target = collapsed ? kCollapsedW : kExpandedW;

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
