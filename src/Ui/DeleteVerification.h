#pragma once

#include <QString>

QString formatDeleteVerificationCode(quint32 value);
QString generateDeleteVerificationCode();
bool deleteVerificationCodeMatches(const QString& expected, const QString& input);
