#include <QtTest>
#include "update/version.h"

class TestUpdateUtils : public QObject {
    Q_OBJECT

private slots:
    void test_parse_version();
    void test_compare_versions();
    void test_is_version_newer();
    void test_invalid_versions();
};

void TestUpdateUtils::test_parse_version()
{
    UpdateUtils::Version v;

    QVERIFY(UpdateUtils::parseVersion("1.4.0", v));
    QCOMPARE(v.major, 1);
    QCOMPARE(v.minor, 4);
    QCOMPARE(v.patch, 0);

    QVERIFY(UpdateUtils::parseVersion("2", v));
    QCOMPARE(v.major, 2);
    QCOMPARE(v.minor, 0);
    QCOMPARE(v.patch, 0);

    QVERIFY(UpdateUtils::parseVersion("1.10", v));
    QCOMPARE(v.minor, 10);

    QVERIFY(UpdateUtils::parseVersion("1.4.0-beta", v));
    QCOMPARE(v.major, 1);
    QCOMPARE(v.minor, 4);

    QVERIFY(UpdateUtils::parseVersion(" 1.2.3 ", v));
    QCOMPARE(v.patch, 3);
}

void TestUpdateUtils::test_compare_versions()
{
    QCOMPARE(UpdateUtils::compareVersions("1.4.0", "1.4.0"), 0);
    QCOMPARE(UpdateUtils::compareVersions("1.4", "1.4.0"), 0);
    QCOMPARE(UpdateUtils::compareVersions("1.4.0", "1.3.9"), 1);
    QCOMPARE(UpdateUtils::compareVersions("1.3.9", "1.4.0"), -1);
    QCOMPARE(UpdateUtils::compareVersions("1.4.0", "2.0.0"), -1);
    QCOMPARE(UpdateUtils::compareVersions("2.0.0", "1.99.99"), 1);
    QCOMPARE(UpdateUtils::compareVersions("1.4.0", "1.4.1"), -1);
    QCOMPARE(UpdateUtils::compareVersions("1.10.0", "1.9.0"), 1);
    QCOMPARE(UpdateUtils::compareVersions("1.4.0-beta", "1.4.0"), 0);
}

void TestUpdateUtils::test_is_version_newer()
{
    QVERIFY(UpdateUtils::isVersionNewer("1.5.0", "1.4.0"));
    QVERIFY(UpdateUtils::isVersionNewer("2.0.0", "1.4.0"));
    QVERIFY(!UpdateUtils::isVersionNewer("1.4.0", "1.4.0"));
    QVERIFY(!UpdateUtils::isVersionNewer("1.3.0", "1.4.0"));
    QVERIFY(!UpdateUtils::isVersionNewer("1.4.0", "1.5.0"));
}

void TestUpdateUtils::test_invalid_versions()
{
    UpdateUtils::Version v;
    QVERIFY(!UpdateUtils::parseVersion("", v));
    QVERIFY(!UpdateUtils::parseVersion("abc", v));
    QVERIFY(!UpdateUtils::parseVersion("1.2.3.4", v));
    QVERIFY(!UpdateUtils::parseVersion("1..2", v));

    // Нераспознанные версии считаются равными (не блокируют обновление)
    QCOMPARE(UpdateUtils::compareVersions("oops", "1.4.0"), 0);
}

QTEST_GUILESS_MAIN(TestUpdateUtils)
#include "test_update_utils.moc"
