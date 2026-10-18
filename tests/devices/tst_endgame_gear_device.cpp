#include "devices/endgame_gear_device.hpp"
#include "devices/endgame_gear_dongle.hpp"
#include "devices/endgame_gear_mouse.hpp"
#include "platform/platform.hpp"

#include <QObject>
#include <QTest>

#include <array>
#include <cstddef>

namespace
{

// Reply layout from docs/ENDGAME.md.
constexpr std::size_t REPLY_SIZE = 64;
constexpr std::size_t STATUS_BYTE = 1;
constexpr std::size_t PERCENT_BYTE = 16;

constexpr BYTE STATUS_OK = 0x01;
constexpr BYTE STATUS_ASLEEP = 0x08;

using Reply = std::array<BYTE, REPLY_SIZE>;

Reply makeReply(BYTE status, BYTE percent)
{
    Reply reply{};
    reply[STATUS_BYTE] = status;
    reply[PERCENT_BYTE] = percent;
    return reply;
}

template <typename Device>
class Probe : public Device
{
public:
    using EndgameGearDevice::DecodeReply;
};

class TestEndgameGearDevice : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsThePercentFromAGoodReply();
    void reportsTheCabledMouseAsCharging();
    void ignoresTheStalePercentOfASleepingMouse();
    void rejectsAnyOtherStatus_data();
    void rejectsAnyOtherStatus();
    void capsThePercentAtOneHundred();
};

void TestEndgameGearDevice::readsThePercentFromAGoodReply()
{
    const Probe<EndgameGearDongle> dongle;

    const auto status = dongle.DecodeReply(makeReply(STATUS_OK, 75).data());

    QCOMPARE(status.percentage, 75);
    QCOMPARE(status.isCharging, false);
}

void TestEndgameGearDevice::reportsTheCabledMouseAsCharging()
{
    const Probe<EndgameGearMouse> mouse;

    const auto status = mouse.DecodeReply(makeReply(STATUS_OK, 75).data());

    QCOMPARE(status.percentage, 75);
    QCOMPARE(status.isCharging, true);
}

// Issue #2: the dongle answers 0x08 for a sleeping mouse and leaves the previous
// reply in the rest of the buffer, so the percent looks valid but is old.
void TestEndgameGearDevice::ignoresTheStalePercentOfASleepingMouse()
{
    const Probe<EndgameGearDongle> dongle;

    const auto status = dongle.DecodeReply(makeReply(STATUS_ASLEEP, 75).data());

    QCOMPARE(status.percentage, -1);
}

void TestEndgameGearDevice::rejectsAnyOtherStatus_data()
{
    QTest::addColumn<BYTE>("status");

    QTest::newRow("0x00") << BYTE{0x00};
    QTest::newRow("0x03") << BYTE{0x03};
    QTest::newRow("0xff") << BYTE{0xFF};
}

void TestEndgameGearDevice::rejectsAnyOtherStatus()
{
    QFETCH(BYTE, status);
    const Probe<EndgameGearDongle> dongle;

    QCOMPARE(dongle.DecodeReply(makeReply(status, 75).data()).percentage, -1);
}

void TestEndgameGearDevice::capsThePercentAtOneHundred()
{
    const Probe<EndgameGearDongle> dongle;

    QCOMPARE(dongle.DecodeReply(makeReply(STATUS_OK, 200).data()).percentage, 100);
}

} // namespace

QTEST_APPLESS_MAIN(TestEndgameGearDevice)

#include "tst_endgame_gear_device.moc"
