#include "PanelSettings.h"

#include <QComboBox>
#include <QSignalBlocker>

namespace PanelSettings {

bool setComboByData(QComboBox* combo, const QVariant& value, int fallbackIndex)
{
    if (!combo) return false;

    const QSignalBlocker blocker(combo);
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
        return true;
    }

    if (combo->count() > 0) {
        combo->setCurrentIndex(qBound(0, fallbackIndex, combo->count() - 1));
    }
    return false;
}

QVariant comboData(const QComboBox* combo)
{
    return combo ? combo->currentData() : QVariant();
}

} // namespace PanelSettings
