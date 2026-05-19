#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QList>

class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

public slots:
    void appendLog(const QString& line);
    void toggleExpanded();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onFilterClicked(const QString& level);

private:
    void setupUi();
    void applyFilter();
    void updateBadgeCounts();

    bool expanded_ = false;
    QString activeFilter_ = "all";

    struct LogEntry {
        QString level; // info, warn, error
        QString text;
        QString time;
    };
    QList<LogEntry> entries_;

    QWidget*       headerWidget_ = nullptr;
    QLabel*        badgeInfo_    = nullptr;
    QLabel*        badgeWarn_    = nullptr;
    QLabel*        badgeError_   = nullptr;
    QPushButton*   filterAll_    = nullptr;
    QPushButton*   filterInfo_   = nullptr;
    QPushButton*   filterWarn_   = nullptr;
    QPushButton*   filterError_  = nullptr;
    QLabel*        chevronLabel_ = nullptr;
    QPlainTextEdit* logView_     = nullptr;

    int countInfo_  = 0;
    int countWarn_  = 0;
    int countError_ = 0;
    int maxLines_   = 5000;
};
