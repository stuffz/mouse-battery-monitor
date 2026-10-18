#include "devices/vaxee_device.hpp"
#include "devices/vaxee_dongle.hpp"
#include "platform/platform.hpp"

#include <QObject>
#include <QTest>

#include <array>
#include <cstddef>
#include <optional>

namespace
{

// Reply layout from docs/VAXEE.md.
constexpr std::size_t REPLY_SIZE = 64;
constexpr std::size_t ECHO_BYTE = 2;
constexpr std::size_t VALUE_BYTE = 5;

constexpr BYTE CMD_BATTERY_LEVEL = 0x0B;

using Reply = std::array<BYTE, REPLY_SIZE>;

Reply makeReply(BYTE echo, BYTE value)
{
    Reply reply{};
    reply[ECHO_BYTE] = echo;
    reply[VALUE_BYTE] = value;
    return reply;
}

class Probe : public VaxeeDongle
{
public:
    using VaxeeDevice::DecodeCharging;
    using VaxeeDevice::DecodeLevel;
};

class TestVaxeeDevice : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void countsTheLevelInStepsOfFive();
    void capsTheLevelAtOneHundred();
    void rejectsAReplyWithoutTheCommandEcho();
    void readsTheChargingFlag();
};

void TestVaxeeDevice::countsTheLevelInStepsOfFive()
{
    QCOMPARE(Probe::DecodeLevel(makeReply(CMD_BATTERY_LEVEL, 15).data()), std::optional<int>(75));
}

void TestVaxeeDevice::capsTheLevelAtOneHundred()
{
    QCOMPARE(Probe::DecodeLevel(makeReply(CMD_BATTERY_LEVEL, 30).data()), std::optional<int>(100));
}

// The device leaves byte[2] zero when it ignored the command.
void TestVaxeeDevice::rejectsAReplyWithoutTheCommandEcho()
{
    QCOMPARE(Probe::DecodeLevel(makeReply(0, 15).data()), std::optional<int>());
}

void TestVaxeeDevice::readsTheChargingFlag()
{
    QVERIFY(Probe::DecodeCharging(makeReply(CMD_BATTERY_LEVEL, 1).data()));
    QVERIFY(!Probe::DecodeCharging(makeReply(CMD_BATTERY_LEVEL, 0).data()));
}

} // namespace

QTEST_APPLESS_MAIN(TestVaxeeDevice)

#include "tst_vaxee_device.moc"
