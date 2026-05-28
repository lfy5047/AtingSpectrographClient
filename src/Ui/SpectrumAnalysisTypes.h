#pragma once

#include <QColor>
#include <QString>

struct SpectrumSampleLine {
    int y = 0;
    QColor color;

    QString name() const
    {
        return QString("y=%1").arg(y);
    }
};
