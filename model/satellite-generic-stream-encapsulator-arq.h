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

#ifndef SATELLITE_GENERIC_STREAM_ENCAPSULATOR_ARQ
#define SATELLITE_GENERIC_STREAM_ENCAPSULATOR_ARQ


#include <map>
#include "ns3/event-id.h"
#include "ns3/mac48-address.h"
#include "satellite-generic-stream-encapsulator.h"
#include "satellite-arq-sequence-number.h"
#include "satellite-arq-buffer-context.h"
#include "satellite-control-message.h"

namespace ns3 {


/**
 * \ingroup satellite
 *
 * \brief SatGenericStreamEncapsulatorArq class is inherited from the
 * SatGenericStreamEncapsulator class, which is used in forward link for
 * encapsulation, fragmentation and packing of higher layer packets.
 * SatGenericStreamEncapsulatorArq includes in addition the retransmission
 * functionality if packets are not received properly in the first
 * transmission. ARQ is controlled by ACK messages replied related to
 * each properly received packet.
 *
 * The SatGenericStreamEncapsulator object is UT specific and its entities
 * are located at both UT (encapsulation, fragmentation, packing) and
 * GW (decapsulation, defragmentation, reassembly).
 */
class SatGenericStreamEncapsulatorArq : public SatGenericStreamEncapsulator
{
public:

  /**
   * Default constructor, not used
   */
  SatGenericStreamEncapsulatorArq ();

  /**
   * Constructor
   * \param source Source MAC address for the encapsulator (UT address)
   * \param dest Destination MAC address for the encapsulator (GW address)
   * \param flowId Flow id of the encapsulator
   */
  SatGenericStreamEncapsulatorArq (Mac48Address source, Mac48Address dest, uint8_t flowId);
  virtual ~SatGenericStreamEncapsulatorArq ();

  static TypeId GetTypeId (void);
  TypeId GetInstanceTypeId (void) const;

  virtual void DoDispose ();

  /**
   * Notify a Tx opportunity to this encapsulator.
   * \param bytes Notified tx opportunity bytes from lower layer
   * \param bytesLeft Bytes left after this TxOpportunity in SatQueue
   * \return A GSE PDU
   */
  virtual Ptr<Packet> NotifyTxOpportunity (uint32_t bytes, uint32_t &bytesLeft);

  /**
   * Receive a packet, thus decapsulate and defragment/deconcatenate
   * if needed. The decapsuled/defragmented HL PDU is forwarded back to
   * LLC and to upper layer.
   * \param A packet pointer received from lower layer
   */
  virtual void ReceivePdu (Ptr<Packet> p);

  /**
   * Receive a control message (ARQ ACK)
   * \param ack Control message pointer received from lower layer
   */
  virtual void ReceiveAck (Ptr<SatArqAckMessage> ack);

  /**
   * Get the buffered packets for this encapsulator
   * \return uint32_t buffered bytes
   */
  virtual uint32_t GetTxBufferSizeInBytes () const;

private:

  /**
   * ARQ Tx timer has expired. The PDU will be flushed, if the maximum
   * retransmissions has been reached. Otherwise the packet will be resent.
   * \param seqNo Sequence number
   */
  void ArqReTxTimerExpired (uint8_t seqNo);

  /**
   * Clean-up a certain sequence number
   * \param sequenceNumber Sequence number
   */
  void CleanUp (uint8_t sequenceNumber);

  /**
   * Convert the 8-bit sequence number value from ARQ header into
   * 32-bit continuous sequence number stream at the receiver.
   * \param seqNo 8-bit sequence number
   * \return uint32_t 32-bit sequence number
   */
  uint32_t ConvertSeqNo (uint8_t seqNo) const;

  /**
   * Reassemble and receive the received PDUs if possible
   */
  void ReassembleAndReceive ();

  /**
   * Rx waiting timer for a PDU has expired
   * \param sn Sequence number
   */
  void RxWaitingTimerExpired (uint32_t sn);

  /**
   * Send ACK for a given sequence number
   * \param seqNo Sequence number
   */
  void SendAck (uint8_t seqNo) const;

  /**
   * Sequence number handler
   */
  Ptr<SatArqSequenceNumber> m_seqNo;

  /**
   * Transmitted and retransmission context buffer
   */
  std::map < uint8_t, Ptr<SatArqBufferContext> > m_txedBuffer;       // Transmitted packets buffer
  std::map < uint8_t, Ptr<SatArqBufferContext> > m_retxBuffer;       // Retransmission buffer
  uint32_t m_retxBufferSize;
  uint32_t m_txedBufferSize;

  /**
   * Maximum number of retransmissions
   */
  uint32_t m_maxNoOfRetransmissions;

  /**
   * Retransmission timer, i.e. when to retransmit a packet if
   * a ACK has not been received.
   */
  Time m_retransmissionTimer;

  /**
   * ARQ window size, i.e. how many sequential sequence numbers may be in
   * use simultaneously.
   */
  uint32_t m_arqWindowSize;

  /**
   * ARQ header size in Bytes
   */
  uint32_t m_arqHeaderSize;

  /**
   * Next expected sequence number at the packet reception
   */
  uint32_t m_nextExpectedSeqNo;

  /**
   * Waiting time for waiting a certain SN to be received.
   */
  Time m_rxWaitingTimer;

  /**
   * key = sequence number
   * value = GSE packet
   */
  std::map<uint32_t, Ptr<SatArqBufferContext> > m_reorderingBuffer;
};


} // namespace ns3

#endif // SATELLITE_GENERIC_STREAM_ENCAPSULATOR_ARQ