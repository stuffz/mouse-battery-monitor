#include "core/config.hpp"

#include <QFile>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QTest>

#include <cstddef>
#include <exception>
#include <string>

namespace
{

constexpr int DEFAULT_INTERVAL_SECONDS = 300;
constexpr int DEFAULT_THRESHOLD = 20;

class TestConfig : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsTheDefaultsWithoutAFile();
    void readsEveryKey();
    void skipsCommentsAndBlankLines();
    void trimsSpacesAndTabs();
    void ignoresUnknownKeys();
    void readsTrueAndOneAsOn_data();
    void readsTrueAndOneAsOn();
    void keepsTheDefaultForABadNumber_data();
    void keepsTheDefaultForABadNumber();

private:
    QTemporaryDir dir;

    std::string write(const char *contents);
};

void TestConfig::keepsTheDefaultsWithoutAFile()
{
    Config config;

    QVERIFY(!config.Load(dir.filePath(QStringLiteral("missing.ini")).toStdString()));

    QCOMPARE(config.GetUpdateIntervalSeconds(), DEFAULT_INTERVAL_SECONDS);
    QCOMPARE(config.GetShowNotifications(), true);
    QCOMPARE(config.GetLowBatteryThreshold(), DEFAULT_THRESHOLD);
    QCOMPARE(config.GetDebugMode(), false);
}

void TestConfig::readsEveryKey()
{
    Config config;

    QVERIFY(config.Load(write("update_interval_seconds=60\n"
                              "show_notifications=false\n"
                              "low_battery_threshold=15\n"
                              "debug_mode=true\n")));

    QCOMPARE(config.GetUpdateIntervalSeconds(), 60);
    QCOMPARE(config.GetShowNotifications(), false);
    QCOMPARE(config.GetLowBatteryThreshold(), 15);
    QCOMPARE(config.GetDebugMode(), true);
}

void TestConfig::skipsCommentsAndBlankLines()
{
    Config config;

    QVERIFY(config.Load(write("# update_interval_seconds=1\n"
                              "; low_battery_threshold=1\n"
                              "\n"
                              "update_interval_seconds=60\n")));

    QCOMPARE(config.GetUpdateIntervalSeconds(), 60);
    QCOMPARE(config.GetLowBatteryThreshold(), DEFAULT_THRESHOLD);
}

void TestConfig::trimsSpacesAndTabs()
{
    Config config;

    QVERIFY(config.Load(write("  update_interval_seconds \t=\t 60  \n")));

    QCOMPARE(config.GetUpdateIntervalSeconds(), 60);
}

void TestConfig::ignoresUnknownKeys()
{
    Config config;

    QVERIFY(config.Load(write("poll_rate=1\nupdate_interval_seconds=60\n")));

    QCOMPARE(config.GetUpdateIntervalSeconds(), 60);
}

void TestConfig::readsTrueAndOneAsOn_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("expected");

    QTest::newRow("true") << QStringLiteral("true") << true;
    QTest::newRow("1") << QStringLiteral("1") << true;
    QTest::newRow("false") << QStringLiteral("false") << false;
    QTest::newRow("0") << QStringLiteral("0") << false;
}

void TestConfig::readsTrueAndOneAsOn()
{
    QFETCH(QString, value);
    QFETCH(bool, expected);
    Config config;

    QVERIFY(config.Load(write(("debug_mode=" + value.toStdString() + "\n").c_str())));

    QCOMPARE(config.GetDebugMode(), expected);
}

// A typo in config.ini must not stop the tray from starting, and "5m" must not
// quietly become five seconds.
void TestConfig::keepsTheDefaultForABadNumber_data()
{
    QTest::addColumn<QString>("value");

    QTest::newRow("word") << QStringLiteral("five");
    QTest::newRow("unit suffix") << QStringLiteral("5m");
    QTest::newRow("empty") << QString();
    QTest::newRow("out of range") << QStringLiteral("99999999999");
}

void TestConfig::keepsTheDefaultForABadNumber()
{
    QFETCH(QString, value);
    Config config;
    const std::string path =
        write(("update_interval_seconds=" + value.toStdString() + "\n").c_str());

    bool threw = false;
    try
    {
        config.Load(path);
    }
    catch (const std::exception &)
    {
        threw = true;
    }

    QVERIFY2(!threw, "Load threw on a bad number");
    QCOMPARE(config.GetUpdateIntervalSeconds(), DEFAULT_INTERVAL_SECONDS);
    QCOMPARE(config.GetErrors().size(), std::size_t{1});
}

std::string TestConfig::write(const char *contents)
{
    const QString path = dir.filePath(QStringLiteral("config.ini"));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qFatal("cannot write %s", qPrintable(path));
    }
    file.write(contents);

    return path.toStdString();
}

} // namespace

QTEST_APPLESS_MAIN(TestConfig)

#include "tst_config.moc"
