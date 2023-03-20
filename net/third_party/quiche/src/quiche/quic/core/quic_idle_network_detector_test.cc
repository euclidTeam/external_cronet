// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_idle_network_detector.h"

#include "quiche/quic/core/quic_one_block_arena.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

class QuicIdleNetworkDetectorTestPeer {
 public:
  static QuicAlarm* GetAlarm(QuicIdleNetworkDetector* detector) {
    return detector->alarm_.get();
  }
};

namespace {

class MockDelegate : public QuicIdleNetworkDetector::Delegate {
 public:
  MOCK_METHOD(void, OnHandshakeTimeout, (), (override));
  MOCK_METHOD(void, OnIdleNetworkDetected, (), (override));
  MOCK_METHOD(void, OnBandwidthUpdateTimeout, (), (override));
};

class QuicIdleNetworkDetectorTest : public QuicTest {
 public:
  QuicIdleNetworkDetectorTest() {
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
    detector_ = std::make_unique<QuicIdleNetworkDetector>(
        &delegate_, clock_.Now(), &arena_, &alarm_factory_,
        /*context=*/nullptr);
    alarm_ = static_cast<MockAlarmFactory::TestAlarm*>(
        QuicIdleNetworkDetectorTestPeer::GetAlarm(detector_.get()));
  }

 protected:
  testing::StrictMock<MockDelegate> delegate_;
  QuicConnectionArena arena_;
  MockAlarmFactory alarm_factory_;

  std::unique_ptr<QuicIdleNetworkDetector> detector_;

  MockAlarmFactory::TestAlarm* alarm_;
  MockClock clock_;
};

TEST_F(QuicIdleNetworkDetectorTest,
       IdleNetworkDetectedBeforeHandshakeCompletes) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(20),
            alarm_->deadline());

  // No network activity for 20s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(20));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest, HandshakeTimeout) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());

  // Has network activity after 15s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  detector_->OnPacketReceived(clock_.Now());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(15),
            alarm_->deadline());
  // Handshake does not complete for another 15s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  EXPECT_CALL(delegate_, OnHandshakeTimeout());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest,
       IdleNetworkDetectedAfterHandshakeCompletes) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(20),
            alarm_->deadline());

  // Handshake completes in 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_->OnPacketReceived(clock_.Now());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(600));
  if (!GetQuicRestartFlag(
          quic_enable_sending_bandwidth_estimate_when_network_idle_v2)) {
    EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
              alarm_->deadline());

    // No network activity for 600s.
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(600));
    EXPECT_CALL(delegate_, OnIdleNetworkDetected());
    alarm_->Fire();
    return;
  }

  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(300),
            alarm_->deadline());

  // No network activity for 300s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(300));
  EXPECT_CALL(delegate_, OnBandwidthUpdateTimeout());
  alarm_->Fire();
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(300),
            alarm_->deadline());

  // No network activity for 600s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(300));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest,
       DoNotExtendIdleDeadlineOnConsecutiveSentPackets) {
  EXPECT_FALSE(alarm_->IsSet());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  EXPECT_TRUE(alarm_->IsSet());

  // Handshake completes in 200ms.
  const bool enable_sending_bandwidth_estimate_when_network_idle =
      GetQuicRestartFlag(
          quic_enable_sending_bandwidth_estimate_when_network_idle_v2);
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_->OnPacketReceived(clock_.Now());
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      enable_sending_bandwidth_estimate_when_network_idle
          ? QuicTime::Delta::FromSeconds(1200)
          : QuicTime::Delta::FromSeconds(600));
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // Sent packets after 200ms.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_->OnPacketSent(clock_.Now(), QuicTime::Delta::Zero());
  const QuicTime packet_sent_time = clock_.Now();
  EXPECT_EQ(packet_sent_time + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  // Sent another packet after 200ms
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(200));
  detector_->OnPacketSent(clock_.Now(), QuicTime::Delta::Zero());
  // Verify network deadline does not extend.
  EXPECT_EQ(packet_sent_time + QuicTime::Delta::FromSeconds(600),
            alarm_->deadline());

  if (!enable_sending_bandwidth_estimate_when_network_idle) {
    // No network activity for 600s.
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(600) -
                       QuicTime::Delta::FromMilliseconds(200));
    EXPECT_CALL(delegate_, OnIdleNetworkDetected());
    alarm_->Fire();
    return;
  }

  // Bandwidth update times out after no network activity for 600s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(600) -
                     QuicTime::Delta::FromMilliseconds(200));
  EXPECT_CALL(delegate_, OnBandwidthUpdateTimeout());
  alarm_->Fire();
  EXPECT_TRUE(alarm_->IsSet());
  EXPECT_EQ(packet_sent_time + QuicTime::Delta::FromSeconds(1200),
            alarm_->deadline());

  // Network idle time out after no network activity for 1200s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1200) -
                     QuicTime::Delta::FromMilliseconds(600));
  EXPECT_CALL(delegate_, OnIdleNetworkDetected());
  alarm_->Fire();
}

TEST_F(QuicIdleNetworkDetectorTest, ShorterIdleTimeoutOnSentPacket) {
  detector_->enable_shorter_idle_timeout_on_sent_packet();
  QuicTime::Delta idle_network_timeout = QuicTime::Delta::Zero();
  if (GetQuicRestartFlag(
          quic_enable_sending_bandwidth_estimate_when_network_idle_v2)) {
    idle_network_timeout = QuicTime::Delta::FromSeconds(60);
  } else {
    idle_network_timeout = QuicTime::Delta::FromSeconds(30);
  }
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(), idle_network_timeout);
  EXPECT_TRUE(alarm_->IsSet());
  const QuicTime deadline = alarm_->deadline();
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(30), deadline);

  // Send a packet after 15s and 2s PTO delay.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(15));
  detector_->OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm does not get extended because deadline is > PTO delay.
  EXPECT_EQ(deadline, alarm_->deadline());

  // Send another packet near timeout and 2 s PTO delay.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(14));
  detector_->OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify alarm does not get extended although it is shorter than PTO.
  EXPECT_EQ(deadline, alarm_->deadline());

  // Receive a packet after 1s.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  detector_->OnPacketReceived(clock_.Now());
  EXPECT_TRUE(alarm_->IsSet());
  // Verify idle timeout gets extended by 30s.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(30),
            alarm_->deadline());

  // Send a packet near timeout.
  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(29));
  detector_->OnPacketSent(clock_.Now(), QuicTime::Delta::FromSeconds(2));
  EXPECT_TRUE(alarm_->IsSet());
  // Verify idle timeout gets extended by 1s.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(2), alarm_->deadline());
}

TEST_F(QuicIdleNetworkDetectorTest, NoAlarmAfterStopped) {
  detector_->StopDetection();

  EXPECT_QUIC_BUG(
      detector_->SetTimeouts(
          /*handshake_timeout=*/QuicTime::Delta::FromSeconds(30),
          /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20)),
      "SetAlarm called after stopped");
  EXPECT_FALSE(alarm_->IsSet());
}

TEST_F(QuicIdleNetworkDetectorTest,
       ResetBandwidthTimeoutWhenHandshakeTimeoutIsSet) {
  if (!GetQuicRestartFlag(
          quic_enable_sending_bandwidth_estimate_when_network_idle_v2)) {
    return;
  }
  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  // The deadline is set based on the bandwidth timeout.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(10),
            alarm_->deadline());

  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::FromSeconds(15),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  // Bandwidth timeout is reset and the deadline is set based on the handshake
  // timeout.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(15),
            alarm_->deadline());

  detector_->SetTimeouts(
      /*handshake_timeout=*/QuicTime::Delta::Infinite(),
      /*idle_network_timeout=*/QuicTime::Delta::FromSeconds(20));
  // The deadline is set based on the bandwidth timeout.
  EXPECT_EQ(clock_.Now() + QuicTime::Delta::FromSeconds(10),
            alarm_->deadline());
}

}  // namespace

}  // namespace test
}  // namespace quic
