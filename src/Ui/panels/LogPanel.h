#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QCheckBox>

class LogPanel : public QWidget {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

public slots:
    void appendLog(const QString& line);

private slots:
    void applyFilter();

private:
    QPlainTextEdit* logView_;
    QLineEdit*      searchEdit_;
    QCheckBox*      chkInfo_;
    QCheckBox*      chkWarn_;
    QCheckBox*      chkError_;
    int maxLines_ = 5000;
};
