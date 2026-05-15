#include "LogPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

LogPanel::LogPanel(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // filter row
    auto* filterRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QString::fromUtf8("搜索..."));
    chkInfo_  = new QCheckBox("Info", this);  chkInfo_->setChecked(true);
    chkWarn_  = new QCheckBox("Warn", this);  chkWarn_->setChecked(true);
    chkError_ = new QCheckBox("Error", this); chkError_->setChecked(true);
    filterRow->addWidget(searchEdit_, 1);
    filterRow->addWidget(chkInfo_);
    filterRow->addWidget(chkWarn_);
    filterRow->addWidget(chkError_);
    root->addLayout(filterRow);

    logView_ = new QPlainTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(maxLines_);
    root->addWidget(logView_, 1);
}

void LogPanel::appendLog(const QString& line)
{
    logView_->appendPlainText(line);
}

void LogPanel::applyFilter()
{
    // simple visibility based filter — future enhancement
}
