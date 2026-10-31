/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

Abstract:

    Golden-value tests for CUBIC. A fixed sequence of ACK, loss, and ECN
    events is replayed and the CongestionWindow after every event is recorded.

    The expected values in kExpectedWindows MUST be recorded from the
    UNMODIFIED cubic.c (before any ABE change). Once recorded, these tests
    must keep passing whenever AbeEnabled is FALSE.

--*/

#include "main.h"
#include <memory>
#include <vector>

extern "C" void CubicCongestionControlInitialize(
    _In_ QUIC_CONGESTION_CONTROL* Cc,
    _In_ const QUIC_SETTINGS_INTERNAL* Settings);

namespace {

constexpr uint16_t kMtu = 1280;
constexpr uint32_t kAckBytes = 1200;
constexpr uint64_t kRttUs = 50000;

//
// Step 1: leave this EMPTY. Run the test once on unmodified code, copy the
// GOLDEN lines it prints into this vector (one value per line, in order),
// then commit. Until it has values, the test is skipped.
//
const std::vector<uint32_t> kExpectedWindows = {
        12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    8624,
    8624,
    8624,
    8754,
    8863,
    8956,
    9035,
    9103,
    9161,
    9211,
    9254,
    9291,
    9323,
    9351,
    9375,
    9396,
    9414,
    9856,
    11088,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    8624,
    9856,
    11088,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    8624,
    8624,
    8624,
    9856,
    11088,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
    12320,
};

void InitMockConnection(QUIC_CONNECTION& Connection)
{
    Connection.Paths.Count = 1;
    Connection.Paths.NextPathId = 1;
    Connection.Paths.Paths[0].Mtu = kMtu;
    Connection.Paths.Paths[0].IsActive = TRUE;
    Connection.Settings.PacingEnabled = FALSE;
    Connection.Settings.HyStartEnabled = FALSE;
    Connection.Settings.NetStatsEventEnabled = FALSE;
    Connection.Send.PeerMaxData = UINT64_MAX;
    Connection.SendBuffer.PostedBytes = 0;
    Connection.SendBuffer.IdealBytes = 0;
}

QUIC_ACK_EVENT MakeAck(uint64_t& Now, uint64_t Pn)
{
    Now += 10000;
    QUIC_ACK_EVENT Ack{};
    Ack.TimeNow = Now;
    Ack.LargestAck = Pn;
    Ack.LargestSentPacketNumber = Pn;
    Ack.NumRetransmittableBytes = kAckBytes;
    Ack.NumTotalAckedRetransmittableBytes = kAckBytes;
    Ack.SmoothedRtt = kRttUs;
    Ack.MinRtt = kRttUs - 5000;
    Ack.MinRttValid = TRUE;
    Ack.AdjustedAckTime = Now;
    return Ack;
}

QUIC_LOSS_EVENT MakeLoss(uint64_t Pn)
{
    QUIC_LOSS_EVENT Loss{};
    Loss.NumRetransmittableBytes = kAckBytes;
    Loss.LargestPacketNumberLost = Pn;
    Loss.LargestSentPacketNumber = Pn;
    Loss.PersistentCongestion = FALSE;
    return Loss;
}

QUIC_ECN_EVENT MakeEcn(uint64_t Pn)
{
    QUIC_ECN_EVENT Ecn{};
    Ecn.LargestPacketNumberAcked = Pn;
    Ecn.LargestSentPacketNumber = Pn;
    return Ecn;
}

} // namespace

//
// Replays: 60 ACKs, a loss at 60, 30 ACKs, an ECN CE at 100, 25 ACKs, an ECN
// CE at 95 (inside the recovery window, must be ignored), 20 ACKs, a loss at
// 160, 20 ACKs. Records CongestionWindow after every event.
//
TEST(CubicGoldenTest, AckLossEcnSequence)
{
    auto ConnPtr = std::make_unique<QUIC_CONNECTION>();
    QUIC_CONNECTION& Connection = *ConnPtr;
    InitMockConnection(Connection);

    QUIC_SETTINGS_INTERNAL Settings{};
    Settings.InitialWindowPackets = 10;

    QUIC_CONGESTION_CONTROL* Cc = &Connection.CongestionControl;
    CubicCongestionControlInitialize(Cc, &Settings);

    std::vector<uint32_t> Windows;
    uint64_t Now = 1000000;
    uint64_t Pn = 0;

    auto AckN = [&](int N) {
        for (int i = 0; i < N; ++i) {
            QUIC_ACK_EVENT Ack = MakeAck(Now, Pn);
            Cc->QuicCongestionControlOnDataAcknowledged(Cc, &Ack);
            Windows.push_back(Cc->Cubic.CongestionWindow);
            ++Pn;
        }
    };
    auto Loss = [&](uint64_t LostPn) {
        QUIC_LOSS_EVENT Ev = MakeLoss(LostPn);
        Cc->QuicCongestionControlOnDataLost(Cc, &Ev);
        Windows.push_back(Cc->Cubic.CongestionWindow);
    };
    auto Ecn = [&](uint64_t EcnPn) {
        QUIC_ECN_EVENT Ev = MakeEcn(EcnPn);
        Cc->QuicCongestionControlOnEcn(Cc, &Ev);
        Windows.push_back(Cc->Cubic.CongestionWindow);
    };

    AckN(60);
    Pn = 60;
    Loss(60);
    AckN(30);            // Pn 61..90
    Ecn(100);
    AckN(25);            // Pn 91..115
    Ecn(95);             // inside recovery window: must not reduce again
    AckN(20);            // Pn 116..135
    Pn = 160;
    Loss(160);
    AckN(20);            // Pn 161..180

    // Print every value so it can be copied into kExpectedWindows.
    for (size_t i = 0; i < Windows.size(); ++i) {
        printf("GOLDEN %zu %u\n", i, Windows[i]);
    }
    printf("GOLDEN ecn_count %u\n", Connection.Stats.Send.EcnCongestionCount);

    if (kExpectedWindows.empty()) {
        GTEST_SKIP() << "golden values not recorded yet; see kExpectedWindows";
    }

    ASSERT_EQ(Windows.size(), kExpectedWindows.size());
    for (size_t i = 0; i < Windows.size(); ++i) {
        ASSERT_EQ(Windows[i], kExpectedWindows[i]) << "event index " << i;
    }
}
