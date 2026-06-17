#pragma once

#include <QVariant>

class QComboBox;

namespace PanelSettings {

bool setComboByData(QComboBox* combo, const QVariant& value, int fallbackIndex = 0);
QVariant comboData(const QComboBox* combo);

} // namespace PanelSettings
