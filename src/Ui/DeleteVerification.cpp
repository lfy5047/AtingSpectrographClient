#include "DeleteVerification.h"

#include <QRandomGenerator>

QString formatDeleteVerificationCode(quint32 value)
{
    return QStringLiteral("%1").arg(value % 10000u, 4, 10, QLatin1Char('0'));
}

QString generateDeleteVerificationCode()
{
    return formatDeleteVerificationCode(QRandomGenerator::global()->bounded(10000u));
}

bool deleteVerificationCodeMatches(const QString& expected, const QString& input)
{
    return input.trimmed() == expected;
}
