#include "LogPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QScrollBar>
#include <QMouseEvent>
#include <QStyle>

LogPanel::LogPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("logPanel");
    setupUi();
}

void LogPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    headerWidget_ = new QWidget(this);
    headerWidget_->setObjectName("logPanelHeader");
    headerWidget_->setFixedHeight(36);
    headerWidget_->setCursor(Qt::PointingHandCursor);

    auto* headerLayout = new QHBoxLayout(headerWidget_);
    headerLayout->setContentsMargins(10, 4, 10, 4);
    headerLayout->setSpacing(8);

    // Log icon (simple document icon via unicode)
    auto* iconLbl = new QLabel(QString::fromUtf8("📋"), headerWidget_);
    iconLbl->setFixedWidth(16);
    headerLayout->addWidget(iconLbl);

    auto* titleLbl = new QLabel(QString::fromUtf8("系统日志"), headerWidget_);
    titleLbl->setObjectName("logPanelTitle");
    headerLayout->addWidget(titleLbl);

    // Badges
    auto makeBadge = [&](const QString& text, const QString& color) -> QLabel* {
        auto* lbl = new QLabel(text, headerWidget_);
        lbl->setObjectName("logBadge");
        lbl->setStyleSheet(QString(
            "font-size: 9pt; font-weight: bold; padding: 1px 6px; border-radius: 10px;"
            "background-color: %1; color: %2;").arg(color).arg(color));
        lbl->setFixedHeight(18);
        return lbl;
    };

    badgeInfo_  = makeBadge("0",  "rgba(76, 142, 247, 0.2)");
    badgeWarn_  = makeBadge("0",  "rgba(210, 153, 34, 0.2)");
    badgeError_ = makeBadge("0",  "rgba(229, 72, 77, 0.2)");
    badgeInfo_->setStyleSheet("font-size: 9pt; font-weight: bold; padding: 1px 6px; border-radius: 10px; background-color: rgba(76, 142, 247, 0.12); color: #4C8EF7;");
    badgeWarn_->setStyleSheet("font-size: 9pt; font-weight: bold; padding: 1px 6px; border-radius: 10px; background-color: rgba(210, 153, 34, 0.12); color: #D29922;");
    badgeError_->setStyleSheet("font-size: 9pt; font-weight: bold; padding: 1px 6px; border-radius: 10px; background-color: rgba(229, 72, 77, 0.12); color: #E5484D;");

    headerLayout->addWidget(badgeInfo_);
    headerLayout->addWidget(badgeWarn_);
    headerLayout->addWidget(badgeError_);

    headerLayout->addStretch();

    // Filter tags
    auto makeFilterTag = [&](const QString& text, const QString& level) -> QPushButton* {
        auto* btn = new QPushButton(text, headerWidget_);
        btn->setObjectName("logFilterTag");
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("level", level);
        connect(btn, &QPushButton::clicked, this, [this, level]() {
            onFilterClicked(level);
        });
        return btn;
    };

    filterAll_   = makeFilterTag(QString::fromUtf8("全部"), "all");
    filterInfo_  = makeFilterTag("INFO",  "info");
    filterWarn_  = makeFilterTag("WARN",  "warn");
    filterError_ = makeFilterTag("ERROR", "error");
    filterAll_->setProperty("active", true);
    filterAll_->style()->polish(filterAll_);

    headerLayout->addWidget(filterAll_);
    headerLayout->addWidget(filterInfo_);
    headerLayout->addWidget(filterWarn_);
    headerLayout->addWidget(filterError_);

    // Chevron
    chevronLabel_ = new QLabel(QString::fromUtf8("▲"), headerWidget_);
    chevronLabel_->setStyleSheet("color: #7D8590; font-size: 10pt;");
    chevronLabel_->setFixedWidth(14);
    headerLayout->addWidget(chevronLabel_);

    root->addWidget(headerWidget_);

    // Log content
    logView_ = new QPlainTextEdit(this);
    logView_->setObjectName("logView");
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(maxLines_);
    logView_->setVisible(false);
    root->addWidget(logView_);

    // Click header to toggle — use event filter for left-click
    headerWidget_->installEventFilter(this);
}

bool LogPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == headerWidget_ && event->type() == QEvent::MouseButtonPress) {
        toggleExpanded();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void LogPanel::toggleExpanded()
{
    expanded_ = !expanded_;
    logView_->setVisible(expanded_);
    chevronLabel_->setText(expanded_
        ? QString::fromUtf8("▼")
        : QString::fromUtf8("▲"));

    if (expanded_) {
        setFixedHeight(200);
        applyFilter();
    } else {
        setFixedHeight(36);
    }
}

void LogPanel::onFilterClicked(const QString& level)
{
    activeFilter_ = level;

    for (auto* btn : { filterAll_, filterInfo_, filterWarn_, filterError_ }) {
        btn->setProperty("active", btn->property("level").toString() == level);
        btn->style()->polish(btn);
    }

    if (expanded_) applyFilter();
}

void LogPanel::appendLog(const QString& line)
{
    QString level = "info";
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");

    if (line.contains("ERROR") || line.contains("error") || line.contains("fail", Qt::CaseInsensitive)) {
        level = "error";
        countError_++;
    } else if (line.contains("WARN") || line.contains("warn") || line.contains("timeout", Qt::CaseInsensitive)) {
        level = "warn";
        countWarn_++;
    } else {
        countInfo_++;
    }

    entries_.append({level, line, timeStr});

    if (entries_.size() > maxLines_) {
        auto old = entries_.first();
        if (old.level == "error") countError_--;
        else if (old.level == "warn") countWarn_--;
        else countInfo_--;
        entries_.removeFirst();
    }

    updateBadgeCounts();

    if (expanded_ && (activeFilter_ == "all" || activeFilter_ == level)) {
        QTextCursor cursor = logView_->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat fmt;

        if (level == "error")
            fmt.setForeground(QColor(0xE5, 0x48, 0x4D));
        else if (level == "warn")
            fmt.setForeground(QColor(0xD2, 0x99, 0x22));
        else
            fmt.setForeground(QColor(0x7D, 0x85, 0x90));

        cursor.insertText(QString("[%1] %2\n").arg(timeStr, line), fmt);
        logView_->setTextCursor(cursor);
    }
}

void LogPanel::applyFilter()
{
    logView_->clear();
    QTextCursor cursor = logView_->textCursor();

    for (const auto& e : entries_) {
        if (activeFilter_ != "all" && e.level != activeFilter_)
            continue;

        QTextCharFormat fmt;
        if (e.level == "error")
            fmt.setForeground(QColor(0xE5, 0x48, 0x4D));
        else if (e.level == "warn")
            fmt.setForeground(QColor(0xD2, 0x99, 0x22));
        else
            fmt.setForeground(QColor(0x7D, 0x85, 0x90));

        cursor.insertText(QString("[%1] %2\n").arg(e.time, e.text), fmt);
    }

    logView_->setTextCursor(cursor);
}

void LogPanel::updateBadgeCounts()
{
    badgeInfo_->setText(QString::number(countInfo_));
    badgeWarn_->setText(QString::number(countWarn_));
    badgeError_->setText(QString::number(countError_));
}
