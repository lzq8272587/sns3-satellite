/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Magister Solutions Ltd.
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

#include <cmath>
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "satellite-request-manager.h"
#include "satellite-enums.h"
#include "satellite-utils.h"

NS_LOG_COMPONENT_DEFINE ("SatRequestManager");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SatRequestManager);


SatRequestManager::SatRequestManager ()
:m_gwAddress (),
 m_lastCno (NAN),
 m_llsConf (),
 m_evaluationInterval (Seconds (0.1)),
 m_rttEstimate (MilliSeconds (560)),
 m_maxPendingCrEntries (0),
 m_gainValueK (1/100),
 m_pendingRbdcRequestsKbps (),
 m_pendingVbdcBytes ()
{
  NS_LOG_FUNCTION (this);

}


SatRequestManager::~SatRequestManager ()
{

}

void
SatRequestManager::Initialize (Ptr<SatLowerLayerServiceConf> llsConf)
{
  NS_LOG_LOGIC (this);

  m_llsConf = llsConf;
  m_gainValueK = 1.0 / (2.0 * m_evaluationInterval.GetSeconds ());
  m_maxPendingCrEntries = (uint32_t)(m_rttEstimate.GetInteger () / m_evaluationInterval.GetInteger ());

  m_pendingRbdcRequestsKbps = std::vector< std::deque<uint32_t> > (m_llsConf->GetDaServiceCount (), std::deque<uint32_t> ());
  m_pendingVbdcBytes = std::vector<uint32_t> (m_llsConf->GetDaServiceCount (), 0);
  m_assignedDaResourcesBytes = std::vector<uint32_t> (m_llsConf->GetDaServiceCount (), 0);
  m_sumVbdcVolumeIn = std::vector<uint32_t> (m_llsConf->GetDaServiceCount (), 0);

  NS_LOG_LOGIC ("Gain value: " << m_gainValueK << ", maxPendinCrEntries: " << m_maxPendingCrEntries);

  // Start the request manager evaluation cycle
  Simulator::ScheduleWithContext (m_nodeInfo->GetNodeId (), m_evaluationInterval, &SatRequestManager::DoPeriodicalEvaluation, this);

  // Start the C/N0 report cycle
  m_cnoReportEvent = Simulator::Schedule (m_cnoReportInterval, &SatRequestManager::SendCnoReport, this);
}

TypeId
SatRequestManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SatRequestManager")
    .SetParent<Object> ()
    .AddConstructor<SatRequestManager> ()
    .AddAttribute( "EvaluationInterval",
                   "Evaluation interval time",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&SatRequestManager::m_evaluationInterval),
                   MakeTimeChecker ())
    .AddAttribute( "CnoReportInterval",
                   "C/NO report interval time",
                    TimeValue (Seconds (0.1)),
                    MakeTimeAccessor (&SatRequestManager::m_cnoReportInterval),
                    MakeTimeChecker ())
    .AddAttribute( "RttEstimate",
                   "Round trip time estimate for request manager",
                   TimeValue (MilliSeconds (560)),
                   MakeTimeAccessor (&SatRequestManager::m_rttEstimate),
                   MakeTimeChecker ())
    .AddTraceSource ("CrTrace",
                     "Capacity request trace",
                     MakeTraceSourceAccessor (&SatRequestManager::m_crTrace))
    .AddTraceSource ("CrTraceLog",
                     "Capacity request trace log",
                     MakeTraceSourceAccessor (&SatRequestManager::m_crTraceLog))
    .AddTraceSource ("RbdcTrace",
                     "Trace for all sent RBDC capacity requests.",
                     MakeTraceSourceAccessor (&SatRequestManager::m_rbdcTrace))
    .AddTraceSource ("VbdcTrace",
                     "Trace for all sent VBDC capacity requests.",
                     MakeTraceSourceAccessor (&SatRequestManager::m_vbdcTrace))
  ;
  return tid;
}

TypeId
SatRequestManager::GetInstanceTypeId (void) const
{
  NS_LOG_FUNCTION (this);

  return GetTypeId ();
}


void SatRequestManager::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  for (CallbackContainer_t::iterator it = m_queueCallbacks.begin();
      it != m_queueCallbacks.end ();
      ++it)
    {
      it->second.Nullify();
    }
  m_queueCallbacks.clear ();

  m_ctrlCallback.Nullify ();

  m_llsConf = NULL;

  Object::DoDispose ();
}

void
SatRequestManager::ReceiveQueueEvent (SatQueue::QueueEvent_t event, uint8_t rcIndex)
{
  NS_LOG_FUNCTION (this << event << rcIndex);

  /**
   * TODO: Request manager does not support yet aperiodical evaluation
   * interval. The needed events from SatBuffer arrive to request manager,
   * but the RBDC / VBDC calculation logic may be messed up if the periodicity
   * is irregular.
   */

  if (event == SatQueue::FIRST_BUFFERED_PKT)
    {
      NS_LOG_LOGIC ("FIRST_BUFFERED_PKT event received from queue: " << (uint32_t)(rcIndex));

      //DoEvaluation (false);
    }
  else if (event == SatQueue::BUFFERED_PKT)
    {
      NS_LOG_LOGIC ("BUFFERED_PKT event received from queue: " << (uint32_t)(rcIndex));
    }
  else
    {
      NS_FATAL_ERROR ("Unsupported queue event received!");
    }
}

void
SatRequestManager::DoPeriodicalEvaluation ()
{
  NS_LOG_FUNCTION (this);

  DoEvaluation (true);

  // Schedule next evaluation interval
  Simulator::Schedule (m_evaluationInterval, &SatRequestManager::DoPeriodicalEvaluation, this);
}

void
SatRequestManager::DoEvaluation (bool periodical)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_LOGIC ("---Start request manager evaluation---");

  bool reset = (periodical = true ? true : false);

  Ptr<SatCrMessage> crMsg = CreateObject<SatCrMessage> ();

  // Go through the RC indices
  for (uint8_t rc = 0; rc < m_llsConf->GetDaServiceCount(); ++rc)
    {
      // Get statistics for LLC/SatQueue
      struct SatQueue::QueueStats_t stats = m_queueCallbacks.at(rc)(reset);

      NS_LOG_LOGIC ("Evaluating the needs for RC: " << (uint32_t)(rc));
      NS_LOG_LOGIC ("Incoming kbitrate: " << stats.m_incomingRateKbps);
      NS_LOG_LOGIC ("Outgoing kbitrate: " << stats.m_outgoingRateKbps);
      NS_LOG_LOGIC ("Volume in bytes: " << stats.m_volumeInBytes);
      NS_LOG_LOGIC ("Volume out bytes: " << stats.m_volumeOutBytes);
      NS_LOG_LOGIC ("Total queue size: " << stats.m_queueSizeBytes);

      // RBDC only
      if (m_llsConf->GetDaRbdcAllowed (rc) && !m_llsConf->GetDaVolumeAllowed (rc))
        {
          NS_LOG_LOGIC ("Evaluating RBDC needs for RC: " << (uint32_t)(rc));
          uint32_t rbdcRateKbps = DoRbdc (rc, stats);

          NS_LOG_LOGIC ("Requested RBCD rate for RC: " << (uint32_t)(rc) << " is " << rbdcRateKbps << " kbps");

          if (rbdcRateKbps > 0)
            {
              std::stringstream ss;
              ss << Simulator::Now ().GetSeconds () << ", "
                 << m_nodeInfo->GetNodeId () << ", "
                 << static_cast<uint32_t> (rc) << ", "
                 << SatEnums::DA_RBDC << ", "
                 << rbdcRateKbps << ", "
                 << stats.m_queueSizeBytes;
              m_crTraceLog (ss.str ());
              m_rbdcTrace (rbdcRateKbps);
            }

          crMsg->AddControlElement (rc, SatEnums::DA_RBDC, rbdcRateKbps);
        }

      // VBDC only
      else if (m_llsConf->GetDaVolumeAllowed (rc) && !m_llsConf->GetDaRbdcAllowed (rc))
        {
          NS_LOG_LOGIC ("Evaluation VBDC for RC: " << (uint32_t)(rc));

          uint32_t vbdcKBytes (0);

          SatEnums::SatCapacityAllocationCategory_t cac = DoVbdc (rc, stats, vbdcKBytes);

          if (vbdcKBytes > 0)
            {
              std::stringstream ss;
              ss << Simulator::Now ().GetSeconds () << ", "
                 << m_nodeInfo->GetNodeId () << ", "
                 << static_cast<uint32_t> (rc) << ", "
                 << cac << ", "
                 << vbdcKBytes << ", "
                 << stats.m_queueSizeBytes;
              m_crTraceLog (ss.str ());
              m_vbdcTrace (vbdcKBytes);
            }

          NS_LOG_LOGIC ("Requested VBCD volume for RC: " << (uint32_t)(rc) << " is " << vbdcKBytes << " KBytes with CAC: " << cac);
          crMsg->AddControlElement (rc, cac, vbdcKBytes);
        }

      // RBDC + VBDC
      else if (m_llsConf->GetDaRbdcAllowed (rc) && m_llsConf->GetDaVolumeAllowed (rc))
        {
          NS_LOG_LOGIC ("Evaluation RBDC+VBDC for RC: " << (uint32_t)(rc));

          /**
           * TODO: add functionality here which handles a RC with both RBDC and
           * VBDC enabled!
           */
          NS_FATAL_ERROR ("Simultaneous RBDC and VBDC for one RC is not currently supported!");
        }
      // No dynamic DA configured
      else
        {
          NS_LOG_LOGIC ("RBDC nor VBDC was configured for RC: " << (uint32_t)(rc));
        }
    }

  // If CR has some control elements with non-zero
  // content, send the CR
  if (crMsg->HasNonZeroContent ())
    {
      NS_LOG_LOGIC ("Send CR");
      SendCapacityRequest (crMsg);
    }

  ResetAssignedResources ();

  NS_LOG_LOGIC ("---End request manager evaluation---");
}


void
SatRequestManager::AddQueueCallback (uint8_t rcIndex, SatRequestManager::QueueCallback cb)
{
  NS_LOG_FUNCTION (this << &cb);
  m_queueCallbacks.insert (std::make_pair<uint8_t, SatRequestManager::QueueCallback> (rcIndex, cb));
}

void
SatRequestManager::SetCtrlMsgCallback (SatRequestManager::SendCtrlCallback cb)
{
  NS_LOG_FUNCTION (this << &cb);
  m_ctrlCallback = cb;
}

void
SatRequestManager::SetGwAddress (Mac48Address address)
{
  NS_LOG_FUNCTION (this << address);
  m_gwAddress = address;
}

void
SatRequestManager::SetNodeInfo (Ptr<SatNodeInfo> nodeInfo)
{
  NS_LOG_FUNCTION (this);

  m_nodeInfo = nodeInfo;
}


void
SatRequestManager::CnoUpdated (uint32_t beamId, Address /*utId*/, Address /*gwId*/, double cno)
{
  NS_LOG_FUNCTION (this << beamId << cno);

  NS_LOG_LOGIC ("C/No updated to request manager: " << cno);

  m_lastCno = cno;
}

void
SatRequestManager::ReSynchronizeVbdc ()
{
  NS_LOG_LOGIC (this);

  ResetAll ();

  for (uint32_t i = 0; i < m_pendingVbdcBytes.size (); ++i)
    {
      m_pendingVbdcBytes.at (i) = 0;
    }
}


uint32_t
SatRequestManager::DoRbdc (uint8_t rc, const SatQueue::QueueStats_t stats)
{
  NS_LOG_FUNCTION (this << rc);

  // Calculate the raw RBDC request
  double coefficient = m_gainValueK / m_evaluationInterval.GetSeconds ();
  double thisRbdcKbits = stats.m_incomingRateKbps * m_evaluationInterval.GetSeconds ();
  double previousRbdcKbits = GetPendingRbdcSumKbps (rc) * m_evaluationInterval.GetSeconds ();

  double reqRbdcKbps (stats.m_incomingRateKbps);
  double totalQueueSizeInKBits = SatUtils::BITS_PER_BYTE * stats.m_queueSizeBytes / 1000.0;

  double rbdcSumKbits = thisRbdcKbits + previousRbdcKbits;
  if (totalQueueSizeInKBits > rbdcSumKbits)
    {
      reqRbdcKbps += coefficient * (totalQueueSizeInKBits - rbdcSumKbits);
    }
  // Else the latter term would be negative

  NS_LOG_LOGIC("Raw RBDC bitrate: " << reqRbdcKbps << " kbps");

  // If CRA enabled, substract the CRA bitrate from the calculated RBDC bitrate
  if (m_llsConf->GetDaConstantAssignmentProvided (rc))
    {
      // If CRA is sufficient, no RBDC needed
      if (reqRbdcKbps <= m_llsConf->GetDaConstantServiceRateInKbps(rc))
        {
          reqRbdcKbps = 0.0;
        }
      // Else reduce the CRA from RBDC request
      else
        {
          reqRbdcKbps -= m_llsConf->GetDaConstantServiceRateInKbps(rc);
        }

      NS_ASSERT (m_llsConf->GetDaConstantServiceRateInKbps(rc) < m_llsConf->GetDaMaximumServiceRateInKbps (rc));

      // CRA + RBDC is too much
      if ((m_llsConf->GetDaConstantServiceRateInKbps(rc) + reqRbdcKbps) > m_llsConf->GetDaMaximumServiceRateInKbps (rc))
        {
          reqRbdcKbps = m_llsConf->GetDaMaximumServiceRateInKbps (rc) - m_llsConf->GetDaConstantServiceRateInKbps(rc);
        }
    }
  // CRA is disabled, but check that RBDC request is not by itself going over max service rate.
  else if (reqRbdcKbps > m_llsConf->GetDaMaximumServiceRateInKbps (rc))
    {
      reqRbdcKbps = m_llsConf->GetDaMaximumServiceRateInKbps (rc);
    }

  NS_LOG_LOGIC("RBDC bitrate after CRA as been taken off: " << reqRbdcKbps << " kbps");

  uint32_t crRbdcKbps = m_llsConf->GetQuantizedRbdcValue (rc, reqRbdcKbps);

  NS_LOG_LOGIC("Quantized RBDC bitrate: " << crRbdcKbps << " kbps");

  UpdatePendingRbdcCounters (rc, crRbdcKbps);

  return crRbdcKbps;
}


SatEnums::SatCapacityAllocationCategory_t
SatRequestManager::DoVbdc (uint8_t rc, const SatQueue::QueueStats_t stats, uint32_t &rcVbdcKBytes)
{
  NS_LOG_FUNCTION (this << rc);

  uint32_t vbdcKBytes (0);

  // Update the counters identifying how much we have been requested, and
  // on the other hand, given.
  UpdatePendingVbdcCounters (rc);

  SatEnums::SatCapacityAllocationCategory_t cac = SatEnums::DA_AVBDC;

  // If there is volume in, there is no need to ask for additional resources
  if (stats.m_volumeInBytes > 0)
    {
      NS_LOG_LOGIC("VBDC volume in for RC: " << (uint32_t)(rc) << ": " << stats.m_volumeInBytes << " Bytes");

      // If we assume that we have received all requested resources,
      // send AVBDC request with total queue size.
      if (m_pendingVbdcBytes.at (rc) == 0)
        {
          cac = SatEnums::DA_AVBDC;
          vbdcKBytes = GetAvbdcKBytes (rc, stats);
        }
      else
        {
          cac = SatEnums::DA_VBDC;
          vbdcKBytes = GetVbdcKBytes (rc, stats);
        }
    }

  // Return the VBDC as referenced uint32_t
  rcVbdcKBytes = vbdcKBytes;

  return cac;
}

uint32_t
SatRequestManager::GetAvbdcKBytes (uint8_t rc, const SatQueue::QueueStats_t stats)
{
  NS_LOG_FUNCTION (this << rc);

  Reset (rc);

  uint32_t craBytes (0);
  uint32_t vbdcBytes = stats.m_queueSizeBytes;
  uint32_t vbdcKBytes (0);

  // If CRA enabled, substract the CRA Bytes from VBDC
  if (m_llsConf->GetDaConstantAssignmentProvided (rc))
    {
      NS_LOG_LOGIC("CRA is enabled together with VBDC for RC: " << rc);

      // Calculate how much bytes would be given to this RC index with configured CRA
      craBytes = (uint32_t)((1000.0 * m_llsConf->GetDaConstantServiceRateInKbps(rc) *
          m_evaluationInterval.GetSeconds ()) / SatUtils::BITS_PER_BYTE);
    }

  // If there is still need for Bytes after CRA
  if (craBytes < vbdcBytes)
    {
      vbdcBytes -= craBytes;

      // Convert Bytes to KBytes and quantize the request to set of
      // predefined values
      vbdcKBytes = (uint32_t)(ceil (vbdcBytes / 1000.0));
      vbdcKBytes = m_llsConf->GetQuantizedVbdcValue (rc, vbdcKBytes);

      // Update the pending counters
      m_pendingVbdcBytes.at (rc) = m_pendingVbdcBytes.at (rc) + 1000 * vbdcKBytes;

      NS_LOG_LOGIC("Pending VBDC bytes: " << (uint32_t)(rc) << ": " << m_pendingVbdcBytes.at (rc)<< " Bytes");

      return vbdcKBytes;
    }

  return 0;
}

uint32_t
SatRequestManager::GetVbdcKBytes (uint8_t rc, const SatQueue::QueueStats_t stats)
{
  NS_LOG_FUNCTION (this << rc);

  uint32_t craBytes (0);
  uint32_t vbdcBytes = stats.m_volumeInBytes;
  uint32_t vbdcKBytes (0);

  // If CRA enabled, substract the CRA Bytes from VBDC
  if (m_llsConf->GetDaConstantAssignmentProvided (rc))
    {
      NS_LOG_LOGIC("CRA is enabled together with VBDC for RC: " << rc);

      // Calculate how much bytes would be given to this RC index with configured CRA
      craBytes = (uint32_t)((1000.0 * m_llsConf->GetDaConstantServiceRateInKbps(rc) *
          m_evaluationInterval.GetSeconds ()) / SatUtils::BITS_PER_BYTE);
    }

  // If there is still need for Bytes after CRA
  if (craBytes < vbdcBytes)
    {
      vbdcBytes -= craBytes;

      // Update the sum of VBDC volume in
      m_sumVbdcVolumeIn.at (rc) = m_sumVbdcVolumeIn.at (rc) + vbdcBytes;

      NS_LOG_LOGIC("VBDC volume after CRA for RC: " << (uint32_t)(rc) << ": " << vbdcBytes << " Bytes");

      // If the sum of volume in is more than the previously requested bytes, then
      // we need to ask some more
      if (m_pendingVbdcBytes.at (rc) < m_sumVbdcVolumeIn.at (rc))
        {
          vbdcBytes = m_sumVbdcVolumeIn.at (rc) - m_pendingVbdcBytes.at (rc);

          // Convert Bytes to KBytes and quantize the request to set of
          // predefined values
          vbdcKBytes = (uint32_t)(ceil (vbdcBytes / 1000.0));
          vbdcKBytes = m_llsConf->GetQuantizedVbdcValue (rc, vbdcKBytes);

          NS_LOG_LOGIC("VBDC bytes after quantization: " << vbdcKBytes << " KBytes");

          // Update the pending counters
          m_pendingVbdcBytes.at (rc) = m_pendingVbdcBytes.at (rc) + 1000 * vbdcKBytes;
        }

      NS_LOG_LOGIC("SumVolumeIn: " << (uint32_t)(rc) << ": " << m_sumVbdcVolumeIn.at (rc) << " Bytes");
      NS_LOG_LOGIC("Pending VBDC bytes: " << (uint32_t)(rc) << ": " << m_pendingVbdcBytes.at (rc)<< " Bytes");
      NS_LOG_LOGIC("VBDC volume after pending: " << (uint32_t)(rc) << ": " << vbdcBytes << " Bytes");
    }

  return vbdcKBytes;
}



uint32_t
SatRequestManager::GetPendingRbdcSumKbps (uint8_t rc) const
{
  NS_LOG_FUNCTION (this << rc);

  uint32_t value (0);

  // Check if the key is found
  std::deque<uint32_t> cont = m_pendingRbdcRequestsKbps.at (rc);

  for (std::deque<uint32_t>::const_iterator it = cont.begin ();
      it != cont.end ();
      ++it)
    {
      value += (*it);
    }

  NS_LOG_LOGIC("Pending RBDC sum for RC: " << (uint32_t)(rc) << " is " << value);

  return value;
}


void
SatRequestManager::UpdatePendingRbdcCounters (uint8_t rc, uint32_t value)
{
  NS_LOG_FUNCTION (this << rc << value);

  m_pendingRbdcRequestsKbps.at(rc).push_back (value);

  // If there are too many entries, pop front
  if (m_pendingRbdcRequestsKbps.at (rc).size () > m_maxPendingCrEntries)
    {
      m_pendingRbdcRequestsKbps.at (rc).pop_front ();
    }

  NS_ASSERT (m_pendingRbdcRequestsKbps.at (rc).size () <= m_maxPendingCrEntries);
}

void
SatRequestManager::UpdatePendingVbdcCounters (uint8_t rc)
{
  NS_LOG_LOGIC (this << rc);

  /*
   * m_pendingVbdcBytes is updated with requested bytes and reduce by allocated
   * bytes via TBTP. This information comes from UT MAC.
   * m_assignedDaResources holds the amount of resources allocated during the previous
   * superframe.
   */

  if (m_pendingVbdcBytes.at (rc) > m_assignedDaResourcesBytes.at (rc))
    {
      m_pendingVbdcBytes.at (rc) = m_pendingVbdcBytes.at (rc) - m_assignedDaResourcesBytes.at (rc);
      m_assignedDaResourcesBytes.at (rc) = 0;
    }
  else
    {
      m_pendingVbdcBytes.at (rc) = 0;
      m_assignedDaResourcesBytes.at (rc) = 0;
    }
}

void
SatRequestManager::SendCapacityRequest (Ptr<SatCrMessage> crMsg)
{
  NS_LOG_FUNCTION (this);

  m_cnoReportEvent.Cancel ();

  if ( !m_ctrlCallback.IsNull ())
    {
      NS_LOG_LOGIC ("Send C/N0 report to GW: " << m_gwAddress);

      m_crTrace (Simulator::Now (), m_nodeInfo->GetMacAddress (), crMsg);

      crMsg->SetCnoEstimate (m_lastCno);
      m_ctrlCallback (crMsg, m_gwAddress);
    }
  else
    {
      NS_FATAL_ERROR ("Unable to send capacity request, since the Ctrl callback is NULL!");
    }

  m_cnoReportEvent = Simulator::Schedule (m_cnoReportInterval, &SatRequestManager::SendCnoReport, this);
}

void
SatRequestManager::SendCnoReport ()
{
  NS_LOG_FUNCTION (this);

  if ( !m_ctrlCallback.IsNull ())
    {
      NS_LOG_LOGIC ("Send C/No report to GW: " << m_gwAddress);

      Ptr<SatCnoReportMessage> cnoReport = CreateObject<SatCnoReportMessage> ();

      cnoReport->SetCnoEstimate (m_lastCno);
      m_ctrlCallback (cnoReport, m_gwAddress);
    }
  else
    {
      NS_FATAL_ERROR ("Unable to send C/N0 report, since the Ctrl callback is NULL!");
    }

  m_cnoReportEvent = Simulator::Schedule (m_cnoReportInterval, &SatRequestManager::SendCnoReport, this);
}

void
SatRequestManager::AssignedDaResources (uint8_t rcIndex, uint32_t bytes)
{
  NS_LOG_FUNCTION (this << rcIndex << bytes);

  NS_LOG_LOGIC ("TBTP resources assigned for RC: " << rcIndex << " bytes: " << bytes);

  m_assignedDaResourcesBytes.at (rcIndex) = m_assignedDaResourcesBytes.at (rcIndex) + bytes;
}

void
SatRequestManager::ResetAssignedResources ()
{
  NS_LOG_FUNCTION (this);

  for (uint32_t i = 0; i < m_llsConf->GetDaServiceCount (); ++i)
    {
      m_assignedDaResourcesBytes.at (i) = 0;
    }
}

void
SatRequestManager::Reset (uint8_t rc)
{
  NS_LOG_FUNCTION (this);

  m_assignedDaResourcesBytes.at (rc) = 0;
  m_pendingVbdcBytes.at (rc) = 0;
  m_sumVbdcVolumeIn.at (rc) = 0;
}

void
SatRequestManager::ResetAll ()
{
  NS_LOG_FUNCTION (this);

  for (uint32_t i = 0; i < m_llsConf->GetDaServiceCount (); ++i)
    {
      Reset (i);
    }
}

}

