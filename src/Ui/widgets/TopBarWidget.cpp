#include "TopBarWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>

static QPixmap makeTopbarIcon(int type, int size = 14)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(0x7D, 0x85, 0x90), 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    float s = size / 14.0f;
    p.scale(s, s);

    switch (type) {
    case 0:
        p.drawEllipse(QPointF(7, 7), 5, 5);
        break;
    case 1: {
        QPainterPath path;
        path.moveTo(8, 1);
        path.lineTo(3, 7);
        path.lineTo(8, 7);
        path.lineTo(6, 13);
        path.lineTo(11, 7);
        path.lineTo(6, 7);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    case 2:
        p.drawRoundedRect(1, 1, 12, 12, 2, 2);
        p.drawEllipse(QPointF(5, 5), 1.5, 1.5);
        p.drawPolyline(QPolygonF() << QPointF(1, 9) << QPointF(9, 5) << QPointF(13, 9));
        break;
    case 3:
        p.drawEllipse(QPointF(7, 7), 5.5, 5.5);
        p.drawLine(7, 4, 7, 8);
        p.drawPoint(7, 10);
        break;
    case 4:
        p.drawEllipse(QPointF(7, 7), 5.5, 5.5);
        p.drawLine(7, 7, 7, 3);
        p.drawLine(7, 7, 10, 8);
        break;
    case 5:
        p.drawRoundedRect(1, 1, 12, 12, 2, 2);
        p.drawLine(1, 5, 13, 5);
        p.drawLine(5, 5, 5, 13);
        break;
    default:
        break;
    }
    p.end();
    return pm;
}

MetricCard::MetricCard(int iconType, const QString& label, QWidget* parent)
    : QFrame(parent)
    , iconType_(iconType)
{
    setObjectName("metricCard");
    setMinimumWidth(86);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 8);
    root->setSpacing(3);

    headerLayout_ = new QHBoxLayout();
    headerLayout_->setSpacing(6);

    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(14, 14);
    iconLabel_->setPixmap(makeTopbarIcon(iconType));
    headerLayout_->addWidget(iconLabel_);

    labelLabel_ = new QLabel(label, this);
    labelLabel_->setObjectName("metricLabel");
    headerLayout_->addWidget(labelLabel_, 1);
    root->addLayout(headerLayout_);

    valueLabel_ = new QLabel("--", this);
    valueLabel_->setObjectName("metricValue");
    root->addWidget(valueLabel_);

    subLabel_ = new QLabel(this);
    subLabel_->setObjectName("metricSub");
    subLabel_->setVisible(false);
    root->addWidget(subLabel_);
}

void MetricCard::setValue(const QString& text)
{
    valueLabel_->setText(text);
}

void MetricCard::setSub(const QString& text)
{
    subLabel_->setText(text);
    subLabel_->setVisible(!text.isEmpty());
}

void MetricCard::setOnline(bool online)
{
    if (online) {
        setProperty("online", true);
        setStyleSheet("#metricCard { border-color: #26A641; }");
        valueLabel_->setStyleSheet("color: #26A641; font-size: 11pt; font-weight: bold;");
    } else {
        setProperty("online", false);
        setStyleSheet("");
        valueLabel_->setStyleSheet("color: #545D68; font-size: 11pt; font-weight: bold;");
    }
}

void MetricCard::addHeaderAction(QWidget* widget)
{
    if (headerLayout_ && widget) {
        headerLayout_->addWidget(widget);
    }
}

TopBarWidget::TopBarWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("topbar");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(110);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 10, 14, 10);
    lay->setSpacing(8);

    connCard_ = new MetricCard(0, QString::fromUtf8("连接状态"), this);
    connCard_->setProperty("connectionCard", true);
    fpsCard_ = new MetricCard(1, QString::fromUtf8("FPS / 丢帧"), this);
    frameCard_ = new MetricCard(2, QString::fromUtf8("总帧数"), this);
    angleCard_ = new MetricCard(4, QString::fromUtf8("转镜角度"), this);
    ipCard_ = new MetricCard(5, QString::fromUtf8("设备 IP"), this);

    fpsCard_->setSub("Raw16 / Slice");

    connectionButton_ = new QPushButton(QString::fromUtf8("连接"), connCard_);
    connectionButton_->setObjectName(QStringLiteral("topBarConnectionButton"));
    connectionButton_->setCursor(Qt::PointingHandCursor);
    connectionButton_->setProperty("compactAction", true);
    connectionButton_->setProperty("primary", true);
    connectionButton_->setMinimumWidth(52);
    connectionButton_->setMaximumWidth(64);
    connectionButton_->setFixedHeight(26);
    connCard_->addHeaderAction(connectionButton_);
    connect(connectionButton_, &QPushButton::clicked,
            this, &TopBarWidget::connectToggleRequested);

    themeCard_ = new QFrame(this);
    themeCard_->setObjectName("themeCard");
    themeCard_->setMinimumWidth(126);
    themeCard_->setMaximumWidth(150);
    auto* themeLayout = new QVBoxLayout(themeCard_);
    themeLayout->setContentsMargins(12, 8, 12, 8);
    themeLayout->setSpacing(6);

    auto* themeLabel = new QLabel(QString::fromUtf8("主题"), themeCard_);
    themeLabel->setObjectName("metricLabel");
    themeLayout->addWidget(themeLabel);

    themeCombo_ = new QComboBox(themeCard_);
    themeCombo_->setObjectName("themeCombo");
    themeCombo_->setCursor(Qt::PointingHandCursor);
    const QVector<ThemeManager::ThemeInfo> themes = ThemeManager::availableThemes();
    for (const ThemeManager::ThemeInfo& info : themes) {
        themeCombo_->addItem(info.displayName, info.id);
    }
    themeLayout->addWidget(themeCombo_);
    connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (updatingThemeCombo_ || index < 0) return;
                const QString id = themeCombo_->itemData(index).toString();
                emit themeChanged(ThemeManager::themeFromId(id));
            });

    lay->addWidget(connCard_, 1);
    lay->addWidget(fpsCard_, 1);
    lay->addWidget(frameCard_, 1);
    lay->addWidget(angleCard_, 1);
    lay->addWidget(ipCard_, 1);
    lay->addWidget(themeCard_);

    setConnected(false);
    setFps(0);
    setFrames(0);
    setDropped(0);
    setMirrorAngle(-1);
}

void TopBarWidget::setTheme(ThemeManager::Theme theme)
{
    if (!themeCombo_) return;

    const QString selectedId = ThemeManager::id(theme);
    updatingThemeCombo_ = true;
    for (int i = 0; i < themeCombo_->count(); ++i) {
        if (themeCombo_->itemData(i).toString() == selectedId) {
            themeCombo_->setCurrentIndex(i);
            break;
        }
    }
    updatingThemeCombo_ = false;
}

void TopBarWidget::setConnected(bool connected, const QString& ip)
{
    connected_ = connected;
    connCard_->setValue(connected ? QString::fromUtf8("已连接") : QString::fromUtf8("未连接"));
    connCard_->setOnline(connected);
    ipCard_->setValue(connected ? ip : "---");
    connectionButton_->setText(connected ? QString::fromUtf8("断开") : QString::fromUtf8("连接"));
    connectionButton_->setProperty("danger", connected);
    connectionButton_->setProperty("primary", !connected);
    connectionButton_->style()->polish(connectionButton_);
}

void TopBarWidget::setFps(double fps)
{
    currentFps_ = fps;
    updateFpsDropped();
}

void TopBarWidget::setFrames(quint64 n)
{
    frameCard_->setValue(n > 0 ? QString::number(n) : "0");
}

void TopBarWidget::setDropped(quint64 n)
{
    currentDropped_ = n;
    updateFpsDropped();
}

void TopBarWidget::setMirrorAngle(double deg)
{
    angleCard_->setValue(deg >= 0 ? QString::number(deg, 'f', 3) + QString::fromUtf8("°")
                                  : QString::fromUtf8("--°"));
}

void TopBarWidget::updateFpsDropped()
{
    const QString fpsText = currentFps_ > 0 ? QString::number(currentFps_, 'f', 1) : "--";
    fpsCard_->setValue(QStringLiteral("%1 / %2").arg(fpsText).arg(currentDropped_));
}
