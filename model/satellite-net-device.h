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

#ifndef SATELLITE_NET_DEVICE_H
#define SATELLITE_NET_DEVICE_H

#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include <stdint.h>
#include <string>
#include "ns3/traced-callback.h"

namespace ns3 {


class SatPhy;
class Node;
class ErrorModel;
class VirtualChannel;

/**
 * \defgroup satellite Satellite Models
 *
 */


/**
 * \ingroup satellite
 * SatNetDevice to be utilized in the UT, GW and satellite.
 */
class SatNetDevice : public NetDevice
{
public:
  static TypeId GetTypeId (void);
  SatNetDevice ();

  /*
   * Receive the packet from the lower layers
   * @param packet Pointer to the packet to be received.
   */
  void Receive (Ptr<Packet> packet);

  /*
   * Attach the SatPhy physical layer to this netdevice.
   * @param phy SatPhy pointer to be added
   */
  void SetPhy (Ptr<SatPhy> phy);

  /**
   * Attach a receive ErrorModel to the SatNetDevice.
   * \param em Ptr to the ErrorModel.
   */
  void SetReceiveErrorModel (Ptr<ErrorModel> em);

  // inherited from NetDevice base class.
  virtual void SetIfIndex (const uint32_t index);
  virtual uint32_t GetIfIndex (void) const;
  virtual Ptr<SatPhy> GetPhy (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);
  virtual bool NeedsArp (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual Address GetMulticast (Ipv6Address addr) const;

  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom (void) const;

  virtual void SetVirtualChannel (Ptr<VirtualChannel>);
  virtual Ptr<Channel> GetChannel (void) const;

protected:
  virtual void DoDispose (void);

private:
  Ptr<SatPhy> m_phy;
  NetDevice::ReceiveCallback m_rxCallback;
  NetDevice::PromiscReceiveCallback m_promiscCallback;
  Ptr<Node> m_node;
  uint16_t m_mtu;
  uint32_t m_ifIndex;
  Mac48Address m_address;
  Ptr<ErrorModel> m_receiveErrorModel;

  /*
   * Virtual channel is used to virtually connect netdevices to each other.
   * This allows the usage of global "automated" routing.
   */
  Ptr<VirtualChannel> m_virtualChannel;

  /**
   * The trace source fired when the phy layer drops a packet it has received
   * due to the error model being active.  Although SatNetDevice doesn't
   * really have a Phy model, we choose this trace source name for alignment
   * with other trace sources.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;
};

} // namespace ns3

#endif /* SATELLITE_NET_DEVICE_H */