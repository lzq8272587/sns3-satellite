/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Magister Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jani Puttonen <jani.puttonen@magister.fi>
 */

/**
 * \file satellite-antenna-pattern-test.cc
 * \brief Test cases to unit test Satellite Antenna Pattern
 */

#include <vector>
#include "ns3/log.h"
#include "ns3/test.h"
#include "ns3/timer.h"
#include "ns3/simulator.h"
#include "../model/satellite-channel-fading-trace-container.h"
#include "../model/satellite-channel.h"


using namespace ns3;

/**
 * \brief Test case to unit test satellite antenna patterns
 *
 */
class SatFadingTraceTestCase : public TestCase
{
public:
  SatFadingTraceTestCase ();
  virtual ~SatFadingTraceTestCase ();

  void TestGetFading (uint32_t nodeId, SatChannel::ChannelType_t channelType);

private:
  virtual void DoRun (void);

  Ptr<SatChannelFadingTraceContainer> m_fadingTraceContainer;

  std::vector<double> m_results;
};

SatFadingTraceTestCase::SatFadingTraceTestCase ()
  : TestCase ("Test satellite antenna gain pattern.")
{
}

SatFadingTraceTestCase::~SatFadingTraceTestCase ()
{
}

void SatFadingTraceTestCase::TestGetFading (uint32_t nodeId, SatChannel::ChannelType_t channelType)
{
  Ptr<SatChannelFadingTrace> trace = m_fadingTraceContainer->GetFadingTrace (nodeId, channelType);
  double fading = trace->GetFading ();
  m_results.push_back (fading);
}


void
SatFadingTraceTestCase::DoRun (void)
{
  // Create antenna gain container
  uint32_t numUts (2);
  uint32_t numGws (5);

  // Read and prepare the fading traces
  m_fadingTraceContainer = CreateObject<SatChannelFadingTraceContainer> (numUts, numGws);

  // Test the fading traces
  bool success = m_fadingTraceContainer->TestFadingTraces ();
  NS_TEST_ASSERT_MSG_EQ( success, true, "SatChannelFadingTrace test failed");

  double time [4] = {1.434, 40.923, 80.503, 140.3};
  double preCalcRes [4] = {1.06879, 1.03526, 1.03093, 1.00159};

  Simulator::Schedule(Seconds(time[0]), &SatFadingTraceTestCase::TestGetFading, this, 1, SatChannel::RETURN_USER_CH);
  Simulator::Schedule(Seconds(time[1]), &SatFadingTraceTestCase::TestGetFading, this, 2, SatChannel::RETURN_FEEDER_CH);
  Simulator::Schedule(Seconds(time[2]), &SatFadingTraceTestCase::TestGetFading, this, 1, SatChannel::FORWARD_USER_CH);
  Simulator::Schedule(Seconds(time[3]), &SatFadingTraceTestCase::TestGetFading, this, 2, SatChannel::FORWARD_FEEDER_CH);

  Simulator::Run ();

  for (uint32_t i = 0; i < 4; ++i)
    {
      NS_TEST_ASSERT_MSG_EQ_TOL(m_results[i], preCalcRes[i], 0.001, "Fading not within expected tolerance");
    }

  Simulator::Destroy ();
}

/**
 * \brief Test suite for Satellite free space loss unit test cases.
 */
class SatFadingTraceSuite : public TestSuite
{
public:
  SatFadingTraceSuite ();
};

SatFadingTraceSuite::SatFadingTraceSuite ()
  : TestSuite ("sat-fading-trace-test", UNIT)
{
  AddTestCase (new SatFadingTraceTestCase);
}

// Do allocate an instance of this TestSuite
static SatFadingTraceSuite satSatInterferenceTestSuite;
