/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 TELEMATICS LAB, DEE - Politecnico di Bari
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
 * Author: Giuseppe Piro  <g.piro@poliba.it>
 *         Marco Miozzo <marco.miozzo@cttc.es>
 *         Nicola Baldo <nbaldo@cttc.es>
 */

#include <ns3/object-factory.h>
#include <ns3/log.h>
#include <math.h>
#include <ns3/simulator.h>
#include <ns3/double.h>
#include "lte-ue-phy.h"
#include "lte-enb-phy.h"
#include "lte-net-device.h"
#include "lte-ue-net-device.h"
#include "lte-enb-net-device.h"
#include "lte-spectrum-value-helper.h"
#include "lte-amc.h"
#include "lte-ue-mac.h"
#include "ff-mac-common.h"
#include "lte-sinr-chunk-processor.h"


NS_LOG_COMPONENT_DEFINE ("LteUePhy");

namespace ns3 {

////////////////////////////////////////
// member SAP forwarders
////////////////////////////////////////


class UeMemberLteUePhySapProvider : public LteUePhySapProvider
{
public:
  UeMemberLteUePhySapProvider (LteUePhy* phy);

  // inherited from LtePhySapProvider
  virtual void SendMacPdu (Ptr<Packet> p);
  virtual void SetBandwidth (uint8_t ulBandwidth, uint8_t dlBandwidth);
  virtual void SendIdealControlMessage (Ptr<IdealControlMessage> msg);

private:
  LteUePhy* m_phy;
};

UeMemberLteUePhySapProvider::UeMemberLteUePhySapProvider (LteUePhy* phy) : m_phy (phy)
{

}


void
UeMemberLteUePhySapProvider::SendMacPdu (Ptr<Packet> p)
{
  m_phy->DoSendMacPdu (p);
}

void
UeMemberLteUePhySapProvider::SetBandwidth (uint8_t ulBandwidth, uint8_t dlBandwidth)
{
  m_phy->DoSetBandwidth (ulBandwidth, dlBandwidth);
}

void
UeMemberLteUePhySapProvider::SendIdealControlMessage (Ptr<IdealControlMessage> msg)
{
  m_phy->DoSendIdealControlMessage (msg);
}



////////////////////////////////////////
// generic LteUePhy methods
////////////////////////////////////////


NS_OBJECT_ENSURE_REGISTERED (LteUePhy);


LteUePhy::LteUePhy ()
{
  NS_LOG_FUNCTION (this);
  NS_FATAL_ERROR ("This constructor should not be called");
}

LteUePhy::LteUePhy (Ptr<LteSpectrumPhy> dlPhy, Ptr<LteSpectrumPhy> ulPhy)
  : LtePhy (dlPhy, ulPhy),
    m_p10CqiPeriocity (MilliSeconds (160)),    
    m_p10CqiLast (MilliSeconds (0)),
    m_a30CqiPeriocity (MilliSeconds (1)), // ideal behavior 
    m_a30CqiLast (MilliSeconds (0))
{
  m_uePhySapProvider = new UeMemberLteUePhySapProvider (this);
}


LteUePhy::~LteUePhy ()
{
}

void
LteUePhy::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete m_uePhySapProvider;
  LtePhy::DoDispose ();
}
  


TypeId
LteUePhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LteUePhy")
    .SetParent<LtePhy> ()
    .AddConstructor<LteUePhy> ()
    .AddAttribute ("TxPower",
                   "Transmission power in dBm",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&LteUePhy::SetTxPower, 
                                       &LteUePhy::GetTxPower),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("NoiseFigure",
                   "Loss (dB) in the Signal-to-Noise-Ratio due to non-idealities in the receiver."
                   " According to Wikipedia (http://en.wikipedia.org/wiki/Noise_figure), this is "
                   "\"the difference in decibels (dB) between"
                   " the noise output of the actual receiver to the noise output of an "
                   " ideal receiver with the same overall gain and bandwidth when the receivers "
                   " are connected to sources at the standard noise temperature T0.\" "
                   "In this model, we consider T0 = 290K.",
                   DoubleValue (9.0),
                   MakeDoubleAccessor (&LteUePhy::SetNoiseFigure, 
                                       &LteUePhy::GetNoiseFigure),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}


void
LteUePhy::SetLteUePhySapUser (LteUePhySapUser* s)
{
  NS_LOG_FUNCTION (this);
  m_uePhySapUser = s;
}

LteUePhySapProvider*
LteUePhy::GetLteUePhySapProvider ()
{
  NS_LOG_FUNCTION (this);
  return (m_uePhySapProvider);
}

void
LteUePhy::SetNoiseFigure (double nf)
{
  NS_LOG_FUNCTION (this << nf);
  m_noiseFigure = nf;
  Ptr<SpectrumValue> noisePsd = LteSpectrumValueHelper::CreateDownlinkNoisePowerSpectralDensity (m_noiseFigure);
  m_downlinkSpectrumPhy->SetNoisePowerSpectralDensity (noisePsd);
}

double
LteUePhy::GetNoiseFigure () const
{
  NS_LOG_FUNCTION (this);
  return m_noiseFigure;
}

void
LteUePhy::SetTxPower (double pow)
{
  NS_LOG_FUNCTION (this << pow);
  m_txPower = pow;
}

double
LteUePhy::GetTxPower () const
{
  NS_LOG_FUNCTION (this);
  return m_txPower;
}

void
LteUePhy::DoSendMacPdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);

  SetMacPdu (p);
}


void
LteUePhy::PhyPduReceived (Ptr<Packet> p)
{
  m_uePhySapUser->ReceivePhyPdu (p);
}

void
LteUePhy::DoSetUplinkSubChannels ()
{
  NS_LOG_FUNCTION (this);

  /*
   *  XXX: the uplink scheduler is not implemented yet!
   *  Now, all uplink sub channels can be used for uplink transmission
   */
  SetSubChannelsForTransmission (GetUplinkSubChannels ());
}


void
LteUePhy::SetSubChannelsForTransmission (std::vector <int> mask)
{
  NS_LOG_FUNCTION (this);

  m_subChannelsForTransmission = mask;

  Ptr<SpectrumValue> txPsd = CreateTxPowerSpectralDensity ();
  m_uplinkSpectrumPhy->SetTxPowerSpectralDensity (txPsd);
}


void
LteUePhy::SetSubChannelsForReception (std::vector <int> mask)
{
  NS_LOG_FUNCTION (this);
  m_subChannelsForReception = mask;
}


std::vector <int>
LteUePhy::GetSubChannelsForTransmission ()
{
  NS_LOG_FUNCTION (this);
  return m_subChannelsForTransmission;
}


std::vector <int>
LteUePhy::GetSubChannelsForReception ()
{
  NS_LOG_FUNCTION (this);
  return m_subChannelsForReception;
}


Ptr<SpectrumValue>
LteUePhy::CreateTxPowerSpectralDensity ()
{
  NS_LOG_FUNCTION (this);
  LteSpectrumValueHelper psdHelper;
  Ptr<SpectrumValue> psd = psdHelper.CreateUplinkTxPowerSpectralDensity (GetTxPower (), GetSubChannelsForTransmission ());

  return psd;
}

void
LteUePhy::GenerateCqiFeedback (const SpectrumValue& sinr)
{
  NS_LOG_FUNCTION (this);
  // check periodic wideband CQI
  if (Simulator::Now () > m_p10CqiLast + m_p10CqiPeriocity)
    {
      Ptr<LteUeNetDevice> thisDevice = GetDevice ()->GetObject<LteUeNetDevice> ();
      Ptr<DlCqiIdealControlMessage> msg = CreateDlCqiFeedbackMessage (sinr);
      DoSendIdealControlMessage (msg);
      m_p10CqiLast = Simulator::Now ();
    }
  // check aperiodic high-layer configured subband CQI
  if  (Simulator::Now () > m_a30CqiLast + m_a30CqiPeriocity)
    {
      Ptr<LteUeNetDevice> thisDevice = GetDevice ()->GetObject<LteUeNetDevice> ();
      Ptr<DlCqiIdealControlMessage> msg = CreateDlCqiFeedbackMessage (sinr);
      DoSendIdealControlMessage (msg);
      m_a30CqiLast = Simulator::Now ();
    }
}



Ptr<DlCqiIdealControlMessage>
LteUePhy::CreateDlCqiFeedbackMessage (const SpectrumValue& sinr)
{
  NS_LOG_FUNCTION (this);

  std::vector<int> cqi = LteAmc::CreateCqiFeedbacks (sinr);

  // CREATE CqiIdealControlMessage
  Ptr<DlCqiIdealControlMessage> msg = Create<DlCqiIdealControlMessage> ();
  CqiListElement_s dlcqi;
  
  if (Simulator::Now () > m_p10CqiLast + m_p10CqiPeriocity)
    {

      int nbSubChannels = cqi.size ();
      double cqiSum = 0.0;
      // average the CQIs of the different RBs
      for (int i = 0; i < nbSubChannels; i++)
        {
          cqiSum += cqi.at (i);
        }
      dlcqi.m_rnti = m_rnti;
      dlcqi.m_ri = 1; // not yet used
      dlcqi.m_cqiType = CqiListElement_s::P10; // Peridic CQI using PUCCH wideband
      dlcqi.m_wbCqi.push_back ((uint16_t) cqiSum / nbSubChannels);
      dlcqi.m_wbPmi = 0; // not yet used
      // dl.cqi.m_sbMeasResult others CQI report modes: not yet implemented
    }
  else if(Simulator::Now () > m_a30CqiLast + m_a30CqiPeriocity)
    {
      int nbSubChannels = cqi.size ();
      int rbgSize = GetRbgSize ();
      double cqiSum = 0.0;
      int cqiNum = 0;
      SbMeasResult_s rbgMeas;
      //NS_LOG_DEBUG (this << " Create A30 CQI feedback, RBG " << rbgSize << " cqiNum " << nbSubChannels << " band "  << (uint16_t)m_dlBandwidth);
      for (int i = 0; i < nbSubChannels; i++)
      {
        cqiSum += cqi.at (i);
        cqiNum++;
        if (cqiNum == rbgSize)
          {
            // average the CQIs of the different RBGs
            //NS_LOG_DEBUG (this << " RBG CQI "  << (uint16_t) cqiSum / rbgSize);
            HigherLayerSelected_s hlCqi;
            hlCqi.m_sbPmi = 0; // not yet used
            hlCqi.m_sbCqi.push_back ((uint16_t) cqiSum / rbgSize);  // only CW0 (SISO mode)
            rbgMeas.m_higherLayerSelected.push_back (hlCqi);
            cqiSum = 0.0;
            cqiNum = 0;
          }
      }
      dlcqi.m_rnti = m_rnti;
      dlcqi.m_ri = 1; // not yet used
      dlcqi.m_cqiType = CqiListElement_s::A30; // Aperidic CQI using PUSCH
      //dlcqi.m_wbCqi.push_back ((uint16_t) cqiSum / nbSubChannels);
      dlcqi.m_wbPmi = 0; // not yet used
      dlcqi.m_sbMeasResult = rbgMeas;
    }
    
  msg->SetDlCqi (dlcqi);
  return msg;
}



void
LteUePhy::DoSendIdealControlMessage (Ptr<IdealControlMessage> msg)
{
  NS_LOG_FUNCTION (this << msg);
  Ptr<LteUeNetDevice> thisDevice = GetDevice ()->GetObject<LteUeNetDevice> ();
  Ptr<LteEnbNetDevice> remoteDevice = thisDevice->GetTargetEnb ();
  msg->SetSourceDevice (thisDevice);
  msg->SetDestinationDevice (remoteDevice);
  SetControlMessages (msg);
}


void
LteUePhy::ReceiveIdealControlMessage (Ptr<IdealControlMessage> msg)
{
  NS_LOG_FUNCTION (this << msg);

  if (msg->GetMessageType () == IdealControlMessage::DL_DCI)
    {
      Ptr<DlDciIdealControlMessage> msg2 = DynamicCast<DlDciIdealControlMessage> (msg);

      DlDciListElement_s dci = msg2->GetDci ();

      if (dci.m_resAlloc != 0)
        {
          NS_FATAL_ERROR ("Resource Allocation type not implemented");
        }

      std::vector <int> dlRb;

      // translate the DCI to Spectrum framework
      uint32_t mask = 0x1;
      for (int i = 0; i < 32; i++)
        {
          if (((dci.m_rbBitmap & mask) >> i) == 1)
            {
              for (int k = 0; k < GetRbgSize (); k++)
                {
                  dlRb.push_back ((i * GetRbgSize ()) + k);
                  //NS_LOG_DEBUG(this << "DL-DCI allocated PRB " << (i*GetRbgSize()) + k);
                }
            }
          mask = (mask << 1);
        }

      SetSubChannelsForReception (dlRb);


    }
  else if (msg->GetMessageType () == IdealControlMessage::UL_DCI) 
    {
      // set the uplink bandwidht according to the UL-CQI
      Ptr<UlDciIdealControlMessage> msg2 = DynamicCast<UlDciIdealControlMessage> (msg);
      UlDciListElement_s dci = msg2->GetDci ();
      std::vector <int> ulRb;
      for (int i = 0; i < dci.m_rbLen; i++)
        {
          ulRb.push_back (i + dci.m_rbStart);
          //NS_LOG_DEBUG (this << " UE RB " << i + dci.m_rbStart);
        }
      SetSubChannelsForTransmission (ulRb);
      // pass the info to the MAC
      m_uePhySapUser->ReceiveIdealControlMessage (msg);
    }
  else
    {
      // pass the message to UE-MAC
      m_uePhySapUser->ReceiveIdealControlMessage (msg);
    }


}


void
LteUePhy::SubframeIndication (uint32_t frameNo, uint32_t subframeNo)
{
  // trigger from eNB
  
  // send control messages
  std::list<Ptr<IdealControlMessage> > ctrlMsg = GetControlMessages ();
  if (ctrlMsg.size () > 0)
    {
      Ptr<LtePhy> phy = GetDevice ()->GetObject<LteUeNetDevice> ()->GetTargetEnb ()->GetPhy ();
      std::list<Ptr<IdealControlMessage> >::iterator it;
      it = ctrlMsg.begin ();
      while (it != ctrlMsg.end ())
        {
          Ptr<IdealControlMessage> msg = (*it);
          phy->ReceiveIdealControlMessage (msg);
          ctrlMsg.pop_front ();
          it = ctrlMsg.begin ();
        }
    }
  
  // send packets in queue
  // send the current burts of packets
  Ptr<PacketBurst> pb = GetPacketBurst ();
  if (pb)
    {
      m_uplinkSpectrumPhy->StartTx (pb);
    }
  
}


void
LteUePhy::SetEnbCellId (uint16_t cellId)
{
  m_enbCellId = cellId;
  m_downlinkSpectrumPhy->SetCellId (cellId);
  m_uplinkSpectrumPhy->SetCellId (cellId);
}



void
LteUePhy::SetRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  m_rnti = rnti;
}



} // namespace ns3
