#include <QtTest/QtTest>

#include "DeleteVerification.h"

class DeleteVerificationTest : public QObject {
    Q_OBJECT

private slots:
    void formatsCodeAsFourDigits();
    void generatedCodeIsFourDigits();
    void matchesTrimmedInputOnly();
    void providesWrongCodeMessage();
};

void DeleteVerificationTest::formatsCodeAsFourDigits()
{
    QCOMPARE(formatDeleteVerificationCode(0), QStringLiteral("0000"));
    QCOMPARE(formatDeleteVerificationCode(7), QStringLiteral("0007"));
    QCOMPARE(formatDeleteVerificationCode(42), QStringLiteral("0042"));
    QCOMPARE(formatDeleteVerificationCode(9999), QStringLiteral("9999"));
    QCOMPARE(formatDeleteVerificationCode(10000), QStringLiteral("0000"));
}

void DeleteVerificationTest::generatedCodeIsFourDigits()
{
    for (int i = 0; i < 100; ++i) {
        const QString code = generateDeleteVerificationCode();
        QCOMPARE(code.size(), 4);
        QVERIFY2(code.contains(QRegularExpression(QStringLiteral("^\\d{4}$"))),
                 qPrintable(QStringLiteral("invalid code: %1").arg(code)));
    }
}

void DeleteVerificationTest::matchesTrimmedInputOnly()
{
    QVERIFY(deleteVerificationCodeMatches(QStringLiteral("1234"), QStringLiteral("1234")));
    QVERIFY(deleteVerificationCodeMatches(QStringLiteral("1234"), QStringLiteral(" 1234 ")));
    QVERIFY(!deleteVerificationCodeMatches(QStringLiteral("1234"), QStringLiteral("123")));
    QVERIFY(!deleteVerificationCodeMatches(QStringLiteral("1234"), QStringLiteral("12345")));
    QVERIFY(!deleteVerificationCodeMatches(QStringLiteral("1234"), QStringLiteral("abcd")));
}

void DeleteVerificationTest::providesWrongCodeMessage()
{
    QCOMPARE(deleteVerificationWrongCodeMessage(), QString::fromUtf8("验证码输入错误"));
}

QTEST_MAIN(DeleteVerificationTest)

#include "DeleteVerificationTest.moc"
