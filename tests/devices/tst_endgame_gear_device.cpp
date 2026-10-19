#include "devices/endgame_gear_device.hpp"
#include "devices/endgame_gear_dongle.hpp"
#include "devices/endgame_gear_mouse.hpp"
#include "platform/platform.hpp"

#include <QObject>
#include <QTest>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace
{

// Request and reply layout from docs/ENDGAME.md.
constexpr std::size_t REPLY_SIZE = 64;
constexpr std::size_t COMMAND_BYTE = 1;
constexpr std::size_t TARGET_BYTE = 2;
constexpr std::size_t STATUS_BYTE = 1;
constexpr std::size_t PERCENT_BYTE = 16;

constexpr BYTE CMD_PROBE = 0x0F;
constexpr BYTE CMD_BATTERY = 0xB4;
constexpr BYTE TARGET_MOUSE = 0x0F;

constexpr BYTE STATUS_OK = 0x01;
constexpr BYTE STATUS_BUSY = 0x03;
constexpr BYTE STATUS_ASLEEP = 0x08;

constexpr BYTE GOOD_PERCENT = 75;

using Reply = std::array<BYTE, REPLY_SIZE>;

Reply makeReply(BYTE status, BYTE percent)
{
    Reply reply{};
    reply[STATUS_BYTE] = status;
    reply[PERCENT_BYTE] = percent;
    return reply;
}

template <typename Device>
class DecodeAccess : public Device
{
public:
    using EndgameGearDevice::DecodeReply;
};

struct SentCommand
{
    BYTE command;
    BYTE target;
};

// Answers the probe with probeStatus and every battery read with a good reply,
// and keeps what was sent.
template <typename Device>
class RecordingDevice : public Device
{
public:
    BYTE probeStatus = STATUS_OK;
    mutable std::vector<SentCommand> sent;

    bool IsConnected() const override { return true; }

protected:
    bool SendReport(const BYTE *buffer, DWORD) const override
    {
        sent.push_back({buffer[COMMAND_BYTE], buffer[TARGET_BYTE]});
        return true;
    }

    bool ReadReport(BYTE, BYTE *buffer, DWORD size) const override
    {
        const bool probed = sent.back().command == CMD_PROBE;
        const Reply reply = probed ? makeReply(probeStatus, 0) : makeReply(STATUS_OK, GOOD_PERCENT);
        std::copy_n(reply.begin(), size, buffer);
        return true;
    }
};

int countCommands(const std::vector<SentCommand> &sent, BYTE command)
{
    return static_cast<int>(std::count_if(sent.begin(), sent.end(), [command](const SentCommand &s)
                                          { return s.command == command; }));
}

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
    void probesTheDongleBeforeReadingTheBattery();
    void skipsTheReadWhenTheProbeFails();
    void doesNotProbeACabledMouse();
};

void TestEndgameGearDevice::readsThePercentFromAGoodReply()
{
    const DecodeAccess<EndgameGearDongle> dongle;

    const auto status = dongle.DecodeReply(makeReply(STATUS_OK, 75).data());

    QCOMPARE(status.percentage, 75);
    QCOMPARE(status.isCharging, false);
}

void TestEndgameGearDevice::reportsTheCabledMouseAsCharging()
{
    const DecodeAccess<EndgameGearMouse> mouse;

    const auto status = mouse.DecodeReply(makeReply(STATUS_OK, 75).data());

    QCOMPARE(status.percentage, 75);
    QCOMPARE(status.isCharging, true);
}

// Issue #2: the dongle answers 0x08 for a sleeping mouse and leaves the previous
// reply in the rest of the buffer, so the percent looks valid but is old.
void TestEndgameGearDevice::ignoresTheStalePercentOfASleepingMouse()
{
    const DecodeAccess<EndgameGearDongle> dongle;

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
    const DecodeAccess<EndgameGearDongle> dongle;

    QCOMPARE(dongle.DecodeReply(makeReply(status, 75).data()).percentage, -1);
}

void TestEndgameGearDevice::capsThePercentAtOneHundred()
{
    const DecodeAccess<EndgameGearDongle> dongle;

    QCOMPARE(dongle.DecodeReply(makeReply(STATUS_OK, 200).data()).percentage, 100);
}

// Endgame's tool probes before every command over the dongle. The probe was
// added after the dongle kept answering 0x08 to a moving mouse until a replug.
void TestEndgameGearDevice::probesTheDongleBeforeReadingTheBattery()
{
    RecordingDevice<EndgameGearDongle> dongle;

    QCOMPARE(dongle.ReadBattery().percentage, int{GOOD_PERCENT});

    QVERIFY(!dongle.sent.empty());
    QCOMPARE(dongle.sent.front().command, CMD_PROBE);
    QCOMPARE(dongle.sent.front().target, TARGET_MOUSE);
    QCOMPARE(countCommands(dongle.sent, CMD_PROBE), 1);
    QCOMPARE(dongle.sent.back().command, CMD_BATTERY);
}

// Endgame's tool skips the command unless the probe answers 0x01.
void TestEndgameGearDevice::skipsTheReadWhenTheProbeFails()
{
    RecordingDevice<EndgameGearDongle> dongle;
    dongle.probeStatus = STATUS_BUSY;

    QCOMPARE(dongle.ReadBattery().percentage, -1);

    QCOMPARE(countCommands(dongle.sent, CMD_BATTERY), 0);
}

// Endgame's tool sends no probe over a cable.
void TestEndgameGearDevice::doesNotProbeACabledMouse()
{
    RecordingDevice<EndgameGearMouse> mouse;

    QCOMPARE(mouse.ReadBattery().percentage, int{GOOD_PERCENT});

    QCOMPARE(countCommands(mouse.sent, CMD_PROBE), 0);
}

} // namespace

QTEST_APPLESS_MAIN(TestEndgameGearDevice)

#include "tst_endgame_gear_device.moc"
