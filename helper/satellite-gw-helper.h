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
 * Author: Sami Rantanen <sami.rantanen@magister.fi>
 */

#ifndef SATELLITE_GW_HELPER_H
#define SATELLITE_GW_HELPER_H

#include <string>

#include "ns3/object-factory.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/traced-callback.h"
#include "ns3/satellite-channel.h"

namespace ns3 {

/**
 * \brief Build a set of SatNetDevice objects
 *
 */
class SatGwHelper : public Object
{
public:
  static TypeId GetTypeId (void);
  TypeId GetInstanceTypeId (void) const;
  /**
   * Create a SatNetDevHelper to make life easier when creating Satellite point to
   * point network connections.
   */
  SatGwHelper ();
  virtual ~SatGwHelper () {}

  /**
   * Each point to point net device must have a queue to pass packets through.
   * This method allows one to set the type of the queue that is automatically
   * created when the device is created and attached to a node.
   *
   * \param type the type of queue
   * \param n1 the name of the attribute to set on the queue
   * \param v1 the value of the attribute to set on the queue
   * \param n2 the name of the attribute to set on the queue
   * \param v2 the value of the attribute to set on the queue
   * \param n3 the name of the attribute to set on the queue
   * \param v3 the value of the attribute to set on the queue
   * \param n4 the name of the attribute to set on the queue
   * \param v4 the value of the attribute to set on the queue
   *
   * Set the type of queue to create and associated to each
   * SatNetDevice created through SatNetDevHelper::Install.
   */
  void SetQueue (std::string type,
                 std::string n1 = "", const AttributeValue &v1 = EmptyAttributeValue (),
                 std::string n2 = "", const AttributeValue &v2 = EmptyAttributeValue (),
                 std::string n3 = "", const AttributeValue &v3 = EmptyAttributeValue (),
                 std::string n4 = "", const AttributeValue &v4 = EmptyAttributeValue ());

  /**
   * Set an attribute value to be propagated to each NetDevice created by the
   * helper.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   *
   * Set these attributes on each ns3::SatNetDevice created
   * by SatNetDevHelper::Install
   */
  void SetDeviceAttribute (std::string name, const AttributeValue &value);

  /**
   * Set an attribute value to be propagated to each Channel created by the
   * helper.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   *
   * Set these attribute on each ns3::SatChannel created
   * by SatNetDevHelper::Install
   */
  void SetChannelAttribute (std::string name, const AttributeValue &value);

  /**
   * \param c a set of nodes
   * \param beamId  id of the beam
   * \param fCh forward channel
   * \param rCh return channel
   *
   * This method creates a ns3::SatChannel with the
   * attributes configured by SatNetDevHelper::SetChannelAttribute,
   * then, for each node in the input container, we create a 
   * ns3::SatNetDevice with the requested attributes,
   * a queue for this ns3::NetDevice, and associate the resulting 
   * ns3::NetDevice with the ns3::Node and ns3::SatChannel.
   */
  NetDeviceContainer Install (NodeContainer c, uint16_t beamId, Ptr<SatChannel> fCh, Ptr<SatChannel> rCh );

  /**
   * \param n node
   * \param beamId  id of the beam
   * \param fCh forward channel
   * \param rCh return channel
   *
   * Saves you from having to construct a temporary NodeContainer.
   */
  Ptr<NetDevice> Install (Ptr<Node> n, uint16_t beamId, Ptr<SatChannel> fCh, Ptr<SatChannel> rCh );

  /**
   * \param aName Name of the node
   * \param beamId  id of the beam
   * \param fCh forward channel
   * \param rCh return channel
   *
   * Saves you from having to construct a temporary NodeContainer.
   */
  Ptr<NetDevice> Install (std::string aName, uint16_t beamId, Ptr<SatChannel> fCh, Ptr<SatChannel> rCh );

  /**
   * Enables creation traces to be written in given file
   * /param stream  stream for creation trace outputs
   * /param cb  callback to connect traces
   */
  void EnableCreationTraces(Ptr<OutputStreamWrapper> stream, CallbackBase &cb);

private:
    /*
     * Beam id is now static and set by the helper for each PHY layer it creates.
     * Note, that this needs to be changed to be read from the reference system
     * configuration table.
     */
    unsigned int m_beamId;

    ObjectFactory m_queueFactory;
    ObjectFactory m_channelFactory;
    ObjectFactory m_deviceFactory;

    /**
     * Trace callback for creation traces
     */
    TracedCallback<std::string> m_creation;
};

} // namespace ns3

#endif /* SATELLITE_GW_HELPER_H */
